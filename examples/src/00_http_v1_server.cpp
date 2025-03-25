//
// Created by usatiynyan.
//

#include "sl/http.hpp"

#include <sl/exec.hpp>
#include <sl/exec/algo/sched/inline.hpp>
#include <sl/exec/algo/sched/manual.hpp>
#include <sl/exec/coro/async.hpp>
#include <sl/exec/thread/pool/monolithic.hpp>
#include <sl/io.hpp>

#include <libassert/assert.hpp>

namespace sl {

http::v1::response_type handle(meta::maybe<http::v1::request_type> http_request) { PANIC("TODO"); }

void main(std::uint16_t port, std::uint16_t max_clients, std::uint32_t tcount) {
    auto epoll = *ASSERT_VAL(io::epoll::create());
    io::socket socket = *ASSERT_VAL(io::socket::create(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0));
    ASSERT(socket.set_opt(SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 1));
    io::socket::bound_server bound_server = *ASSERT_VAL(std::move(socket).bind(AF_INET, port, INADDR_ANY));
    io::socket::server server = *ASSERT_VAL(std::move(bound_server).listen(max_clients));
    io::async_epoll async_epoll{ epoll, server };

    auto client_handle_coro = [](io::async_connection::view conn, exec::executor& executor) -> exec::async<void> {
        using exec::operator co_await;
        using exec::operator|;

        std::array<std::byte, 1024> read_buffer{};
        while (true) {
            std::vector<std::byte> request_buffer;
            {
                while (true) {
                    const auto read_result = co_await (conn.read(read_buffer) | exec::continue_on(executor));
                    if (!read_result.has_value() || read_result.value() == 0) {
                        break;
                    }
                    const std::span request_chunk{ read_buffer.data(), read_result.value() };
                    request_buffer.insert(request_buffer.end(), request_chunk.begin(), request_chunk.end());
                }
            }

            const meta::maybe<http::v1::request_type> maybe_http_request =
                http::v1::parse(std::span<const std::byte>{ request_buffer });

            const http::v1::response_type http_response = handle(maybe_http_request);

            const std::vector<std::byte> response_buffer = http::v1::serialize(http_response);

            {
                std::span<const std::byte> write_buffer{ response_buffer };
                while (!write_buffer.empty()) {
                    const auto write_result = co_await (conn.write(write_buffer) | exec::continue_on(executor));
                    if (!write_result.has_value() || write_result.value() == 0) {
                        break;
                    }
                    write_buffer = write_buffer.subspan(write_result.value());
                }
            }
        }
    };

    auto server_coro = [&async_epoll, client_handle_coro](exec::executor& executor) -> exec::async<void> {
        auto serve = async_epoll.serve_coro(/*check_running=*/[] { return true; });
        while (auto maybe_connection = co_await serve) {
            auto& connection = maybe_connection.value();
            exec::coro_schedule(executor, client_handle_coro(connection, executor));
        }
        const auto ec = std::move(serve).result_or_throw();
        ASSERT(!ec);
    };

    const std::unique_ptr<exec::executor> executor =
        tcount == 0 ? std::unique_ptr<exec::executor>(new exec::detail::inline_executor)
                    : std::unique_ptr<exec::executor>(new exec::monolithic_thread_pool{
                          exec::thread_pool_config::with_hw_limit(tcount) });

    exec::coro_schedule(exec::inline_executor(), server_coro(*executor));

    std::array<::epoll_event, 1024> events{};
    while (async_epoll.wait_and_fulfill(events, tl::nullopt)) {}
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
