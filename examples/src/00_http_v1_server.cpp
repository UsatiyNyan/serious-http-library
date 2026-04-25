//
// Created by usatiynyan.
//

#include "sl/http.hpp"

#include <sl/exec.hpp>
#include <sl/exec/algo/sched/manual.hpp>
#include <sl/exec/coro/async.hpp>
#include <sl/exec/thread/pool/monolithic.hpp>
#include <sl/io.hpp>

#include <sl/io/async/socket.hpp>
#include <sl/meta/assert.hpp>

namespace sl {

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

    while (true) {
        auto request_agen = http::v1::deserialize::request(read_agen(client, executor));
        while (auto request_chunk = co_await request_agen) {
            // TODO: handle chunks
        }
        auto io_result = std::move(request_agen).result_or_throw();
        if (!io_result.has_value()) {
            PANIC("io:", io_result.error().message());
        }
        auto request_result = std::move(io_result).value();
        if (!request_result.has_value()) {
            PANIC("status:", static_cast<std::uint16_t>(request_result.error()));
        }
        auto request = std::move(request_result).value();
        // TODO: handle
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
