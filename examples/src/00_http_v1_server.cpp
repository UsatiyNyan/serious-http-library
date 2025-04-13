//
// Created by usatiynyan.
//

#include "sl/http.hpp"
#include "sl/http/v1/deserialize.hpp"

#include <sl/exec.hpp>
#include <sl/exec/algo/sched/inline.hpp>
#include <sl/exec/algo/sched/manual.hpp>
#include <sl/exec/coro/async.hpp>
#include <sl/exec/thread/pool/monolithic.hpp>
#include <sl/io.hpp>

#include <libassert/assert.hpp>

namespace sl {

exec::async<http::v1::response_message>
    handle(meta::result<http::v1::request_message, http::v1::status_type> http_request_result) {
    PANIC("TODO");
}

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

        // explicitly not default-initialized, safety provided via span
        std::array<std::byte, 1024> read_buffer;
        http::v1::request_deserializer deserializer;
        while (deserializer.keep_alive()) {
            while (!deserializer.has_next()) {
                const auto read_result = co_await (conn.read(read_buffer) | exec::continue_on(executor));
                if (read_result.value_or(0) == 0) {
                    deserializer.close();
                    break;
                }
                const std::span request_chunk{ read_buffer.data(), read_result.value() };
                deserializer.read(request_chunk);
            }

            while (deserializer.has_next()) {
                const auto http_response = co_await handle(deserializer.next());
                // body can be serialized separately, maybe, some optimizations could be applied
                const std::vector<std::byte> response_buffer = http::v1::serialize(http_response);

                std::span<const std::byte> write_buffer{ response_buffer };
                while (!write_buffer.empty()) {
                    const auto write_result = co_await (conn.write(write_buffer) | exec::continue_on(executor));
                    if (write_result.value_or(0) == 0) {
                        deserializer.close();
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
