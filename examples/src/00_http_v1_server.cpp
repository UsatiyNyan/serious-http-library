//
// Created by usatiynyan.
//

#include "sl/http.hpp"
#include "sl/http/v1/detail/strings.hpp"
#include "sl/http/v1/types.hpp"

#include <fmt/base.h>
#include <fmt/format.h>
#include <sl/exec.hpp>
#include <sl/exec/algo/sched/manual.hpp>
#include <sl/exec/coro/async.hpp>
#include <sl/exec/thread/pool/monolithic.hpp>
#include <sl/io.hpp>

#include <sl/io/async/socket.hpp>
#include <sl/meta/assert.hpp>
#include <sl/meta/enum/to_string.hpp>
#include <sl/meta/match/overloaded.hpp>

namespace sl {

std::atomic<std::size_t> request_counter = 0;

std::string format_query(const http::v1::query_params& query) {
    if (query.empty()) {
        return "";
    }
    std::string result = "?";
    for (std::size_t i = 0; i < query.size(); ++i) {
        if (i > 0) {
            result += "&";
        }
        result += fmt::format("{}={}", query[i].first, query[i].second);
    }
    return result;
}

std::string format_target(const http::v1::target_type& target) {
    return std::visit(
        meta::overloaded{
            [](const http::v1::origin_target_type& t) { return fmt::format("{}{}", t.path, format_query(t.query)); },
            [](const http::v1::absolute_target_type& t) {
                return fmt::format("{}://{}{}{}", t.scheme, t.authority, t.path, format_query(t.query));
            },
            [](const http::v1::authority_target_type& t) { return fmt::format("{}:{}", t.host, t.port); },
            [](const http::v1::asterisk_target_type&) { return std::string{ "*" }; },
        },
        target
    );
}

void debug_print(const http::v1::message_type& request) {
    const auto& req = std::get<http::v1::request_line_type>(request.start_line);
    fmt::println("=== Request #{} ===", request_counter.fetch_add(1));
    fmt::println("{} {}", enum_to_str(req.method), format_target(req.target));
    for (const auto& [name, value] : request.fields) {
        fmt::println("{}: {}", name, value);
    }
    if (!request.body.empty()) {
        fmt::println("\n{}", http::v1::detail::buffer_byte_to_str(request.body));
    }
    fmt::println("================");
}

http::v1::message_type handle(const http::v1::message_type& request) {
    debug_print(request);

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

exec::async_gen<std::span<const std::byte>, std::error_code>
    read_agen(sl::io::async::socket::bound& client, exec::executor& executor) {
    using exec::operator co_await;
    using exec::operator|;

    while (true) {
        std::array<std::byte, 1024> buffer{};
        const auto read_result = co_await (client.read(buffer) | exec::continue_on(executor));
        if (!read_result.has_value()) {
            co_return read_result.error();
        }
        co_yield std::span{ buffer.data(), *read_result };
    }
    co_return std::error_code{};
}

exec::async<void> client_coro(
    std::pair<io::sys::socket, io::sys::address> accepted,
    io::sys::epoll& epoll,
    exec::executor& executor
) {
    using exec::operator co_await;
    using exec::operator|;

    auto& [socket, address] = accepted;
    io::state::socket socket_state{ socket };
    auto socket_async = *io::async::socket::create(socket_state);
    auto bound_socket_async = *socket_async->bind(epoll);
    meta::defer unbind_socket{ [&bound_socket_async] { ASSERT(std::move(bound_socket_async).unbind()); } };
    auto& client = bound_socket_async;

    http::v1::deserialize::config_type config{};
    for (std::size_t i = 0; i < 1; ++i) {
        auto reader = read_agen(client);
        auto request_agen = http::v1::deserialize::request(std::move(reader), config);
        while (auto request_chunk = co_await request_agen) {
            fmt::println("chunk?");
            // TODO: handle chunks
        }
        auto io_result = std::move(request_agen).result_or_throw();
        if (!io_result.has_value()) {
            if (io_result.error() == std::errc::connection_reset) {
                break;
            }
            PANIC("io:", io_result.error().message());
        }
        auto request_result = std::move(io_result).value();
        if (!request_result.has_value()) {
            PANIC("status:", static_cast<std::uint16_t>(request_result.error()));
        }
        const auto request = std::move(request_result).value();
        const auto response = handle(request);
        const auto serialize_result = co_await http::v1::serialize::message(response, client);
        if (serialize_result.ec) {
            if (serialize_result.ec == std::errc::connection_reset || serialize_result.ec == std::errc::broken_pipe) {
                break;
            }
            PANIC("serialize:", serialize_result.ec.message());
        }
    }
}

exec::async<void> serve_coro(io::async::server::bound server, io::sys::epoll& epoll, exec::executor& executor) {
    using exec::operator co_await;

    meta::defer unbind_server{ [&server] { ASSERT(std::move(server).unbind()); } };

    while (auto accept_result = co_await server.accept()) {
        exec::coro_schedule(executor, client_coro(std::move(accept_result).value(), epoll, executor));
    }
}

void main(std::uint16_t port, std::uint16_t max_clients, std::uint32_t tcount) {
    auto socket = *ASSERT_VAL(io::sys::socket::create(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0));
    ASSERT(socket.set_opt(SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 1));
    const auto address = io::sys::make_ipv4_address(AF_INET, port, INADDR_ANY);
    auto bound_server = *ASSERT_VAL(std::move(socket).bind(address));
    auto server = *ASSERT_VAL(std::move(bound_server).listen(max_clients));

    io::state::server server_state{ server };
    auto server_async = *ASSERT_VAL(io::async::server::create(server_state));

    auto epoll = *ASSERT_VAL(io::sys::epoll::create());
    auto bound_server_async = *ASSERT_VAL(server_async->bind(epoll));

    std::unique_ptr<exec::executor> executor_holder;
    if (tcount > 0) {
        executor_holder =
            std::make_unique<exec::monolithic_thread_pool<>>(exec::thread_pool_config::with_hw_limit(tcount));
    }
    exec::executor& executor = executor_holder ? *executor_holder : exec::inline_executor();

    exec::coro_schedule(executor, serve_coro(std::move(bound_server_async), epoll, executor));

    std::array<::epoll_event, 1024> events{};
    while (epoll.wait_and_fulfill(events, meta::null)) {}
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
