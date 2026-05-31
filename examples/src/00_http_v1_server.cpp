//
// Created by usatiynyan.
//

#include "sl/http.hpp"
#include "sl/http/v1/deserialize/message.hpp"
#include "sl/http/v1/detail/strings.hpp"
#include "sl/http/v1/serialize/target.hpp"

#include <sl/exec.hpp>
#include <sl/io.hpp>
#include <sl/meta.hpp>

#include <fmt/base.h>
#include <fmt/format.h>

#include <bits/chrono.h>

namespace sl {

using exec::operator co_await;
using exec::operator|;

void debug_print(const http::v1::message_type& request) {
    const auto& req = std::get<http::v1::request_line_type>(request.start_line);
    fmt::println("=== Request # ===");
    fmt::println("{} {}", enum_to_str(req.method), http::v1::serialize(req.target));
    for (const auto& [name, value] : request.fields) {
        fmt::println("{}: {}", name, value);
    }
    if (!request.body.empty()) {
        fmt::println("\n{}", http::v1::detail::buffer_byte_to_str(request.body));
    }
    fmt::println("================");
}

http::v1::message_type handle(const http::v1::message_type& request, bool is_logging) {
    if (is_logging) {
        debug_print(request);
    }

    constexpr std::string_view body_str = "Hello, World!\n";

    http::v1::body_type body{
        std::bit_cast<const std::byte*>(body_str.data()),
        std::bit_cast<const std::byte*>(body_str.data() + body_str.size()),
    };

    http::v1::fields_type fields;
    fields["Content-Type"] = "text/plain";
    fields["Content-Length"] = std::to_string(body.size());
    fields["Connection"] = "keep-alive";

    return http::v1::message_type{
        .fields = std::move(fields),
        .body = std::move(body),
        .start_line =
            http::v1::response_line_type{
                .status = http::v1::status_type::OK,
                .version = http::v1::version_type::HTTPv1_1,
            },
    };
}

static constexpr std::size_t BUFFER_SIZE = 1024;

exec::async<void> client_coro(
    std::pair<io::sys::socket, io::sys::address> accepted,
    io::sys::epoll& epoll,
    exec::serial_executor<>& executor,
    bool is_logging
) {
    auto& [socket, address] = accepted;
    auto socket_async = *io::async::socket::create(socket, epoll);

    meta::maybe<http::v1::message_type> maybe_request = meta::null;

    {
        auto deserialize = http::v1::make_deserialize_request(
            http::v1::deserialize_config{
                .message_cb =
                    [&maybe_request](http::v1::message_type request) { maybe_request.emplace(std::move(request)); },
            }
        );

        std::array<std::byte, BUFFER_SIZE> read_buffer{};
        while (!maybe_request.has_value()) {
            co_await exec::start_on(executor);
            const auto io_result = co_await socket_async->read(read_buffer);
            co_await exec::start_on(executor.get_inner());
            if (!io_result.has_value()) {
                if (io_result.error() == std::errc::connection_reset) {
                    co_return;
                }
                PANIC("read io:", io_result.error().message());
            }
            const auto input = std::span{ read_buffer }.subspan(0, io_result.value());
            const auto maybe_error = deserialize(input);
            if (maybe_error.has_value()) {
                PANIC("deserialize status:", static_cast<std::uint16_t>(maybe_error.value()));
            }
        }
    }

    const auto response = handle(maybe_request.value(), is_logging);

    {
        auto serialize = http::v1::make_serialize(
            response,
            http::v1::serialize_config{
                .buffer_size = BUFFER_SIZE,
            }
        );

        std::size_t written = 0;
        while (true) {
            const auto write_buffer = serialize(written);
            if (write_buffer.empty()) {
                break;
            }
            co_await exec::start_on(executor);
            const auto io_result = co_await socket_async->write(write_buffer);
            co_await exec::start_on(executor.get_inner());
            if (!io_result.has_value()) {
                if (io_result.error() == std::errc::connection_reset || io_result.error() == std::errc::broken_pipe) {
                    co_return;
                }
                PANIC("serialize io:", io_result.error().message());
            }
            written = io_result.value();
        }
    }
}

exec::async<void> serve_coro(
    std::unique_ptr<io::async::server> server_async,
    io::sys::epoll& epoll,
    exec::serial_executor<>& executor,
    bool is_logging
) {
    using exec::operator co_await;

    while (true) {
        co_await exec::start_on(executor);
        auto accept_result = co_await server_async->accept();
        co_await exec::start_on(executor.get_inner());
        exec::coro_schedule(
            executor.get_inner(), client_coro(std::move(accept_result).value(), epoll, executor, is_logging)
        );
    }
}

void main(std::uint16_t port, std::uint16_t max_clients, std::uint32_t tcount, bool is_logging) {
    auto epoll = *ASSERT_VAL(io::sys::epoll::create());

    auto socket = *ASSERT_VAL(io::sys::socket::create(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0));
    ASSERT(socket.set_opt(SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 1));
    const auto address = io::sys::make_ipv4_address(AF_INET, port, INADDR_ANY);
    auto bound_server = *ASSERT_VAL(std::move(socket).bind(address));
    auto server = *ASSERT_VAL(std::move(bound_server).listen(max_clients));
    auto server_async = *ASSERT_VAL(io::async::server::create(server, epoll));

    std::unique_ptr<exec::executor> executor_holder;
    if (tcount > 0) {
        executor_holder =
            std::make_unique<exec::monolithic_thread_pool<>>(exec::thread_pool_config::with_hw_limit(tcount));
    }
    exec::serial_executor<> executor{ executor_holder ? *executor_holder : exec::inline_executor() };

    exec::coro_schedule(executor.get_inner(), serve_coro(std::move(server_async), epoll, executor, is_logging));

    while (true) {
        std::array<::epoll_event, 1024> events{};
        const auto wait_result = epoll.wait(events, meta::null);
        if (!wait_result.has_value()) {
            break;
        }
        const std::uint32_t nevents = wait_result.value();

        exec::schedule(executor, [events, nevents]() mutable {
            io::sys::epoll::fulfill(std::span{ events }.subspan(0, nevents));
            return meta::ok(meta::unit{});
        }) | exec::detach();
    }
}

} // namespace sl

auto parse_args(int argc, const char* argv[]) {
    ASSERT(argc == 5, "port, max_clients, tcount, logging");
    return std::make_tuple(
        static_cast<std::uint16_t>(std::strtoul(argv[1], nullptr, 10)),
        static_cast<std::uint16_t>(std::strtoul(argv[2], nullptr, 10)),
        static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10)),
        static_cast<std::uint16_t>(std::strtoul(argv[4], nullptr, 10))
    );
}

int main(int argc, const char* argv[]) {
    const auto [port, max_clients, tcount, is_logging] = parse_args(argc, argv);
    sl::main(port, max_clients, tcount, is_logging == 1);
    return 0;
}
