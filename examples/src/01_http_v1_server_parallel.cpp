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

http::v1::message_type handle(const http::v1::message_type&) {
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

exec::async<void> client_coro(std::pair<io::sys::socket, io::sys::address> accepted, io::sys::epoll& epoll) {
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
            const auto io_result = co_await socket_async->read(read_buffer);
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

    const auto response = handle(maybe_request.value());

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
            const auto io_result = co_await socket_async->write(write_buffer);
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
    exec::serial_executor<>& executor
) {
    while (true) {
        auto accept_result = co_await server_async->accept();
        exec::coro_schedule(executor.get_inner(), client_coro(std::move(accept_result).value(), epoll));
    }
}

void main(std::uint16_t port, std::uint16_t max_clients, std::uint32_t tcount) {
    auto epoll = *ASSERT_VAL(io::sys::epoll::create());

    auto socket = *ASSERT_VAL(io::sys::socket::create(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0));
    ASSERT(socket.set_opt(SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 1));
    const auto address = io::sys::make_ipv4_address(AF_INET, port, INADDR_ANY);
    auto bound_server = *ASSERT_VAL(std::move(socket).bind(address));
    auto server = *ASSERT_VAL(std::move(bound_server).listen(max_clients));
    auto server_async = *ASSERT_VAL(io::async::server::create(server, epoll));

    std::unique_ptr<exec::executor> executor_holder;
    const auto executor_config = exec::thread_pool_config::with_hw_limit(tcount);
    if (tcount > 0) {
        executor_holder = std::make_unique<exec::monolithic_thread_pool<>>(executor_config);
    }
    exec::serial_executor<> executor{ executor_holder ? *executor_holder : exec::inline_executor() };

    exec::coro_schedule(executor.get_inner(), serve_coro(std::move(server_async), epoll, executor));

    const auto worker = [&epoll] {
        while (true) {
            std::array<::epoll_event, 1024> events{};
            const auto wait_result = epoll.wait(events, meta::null);
            if (!wait_result.has_value()) {
                break;
            }
            const std::uint32_t nevents = wait_result.value();
            const auto events_view = std::span{ events }.subspan(0, nevents);
            io::sys::epoll::fulfill(events_view);
        }
        return meta::ok(meta::unit{});
    };

    const auto actual_tcount = std::max(executor_config.tcount, 1u);
    for (std::uint32_t i = 0; i < actual_tcount - 1; ++i) {
        exec::schedule(executor, worker) | exec::detach();
    }

    worker();
}

} // namespace sl

auto parse_args(int argc, const char* argv[]) {
    ASSERT(argc == 4, "port, max_clients, tcount");
    return std::make_tuple(
        static_cast<std::uint16_t>(std::strtoul(argv[1], nullptr, 10)),
        static_cast<std::uint16_t>(std::strtoul(argv[2], nullptr, 10)),
        static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10))
    );
}

int main(int argc, const char* argv[]) {
    const auto [port, max_clients, tcount] = parse_args(argc, argv);
    sl::main(port, max_clients, tcount);
    return 0;
}
