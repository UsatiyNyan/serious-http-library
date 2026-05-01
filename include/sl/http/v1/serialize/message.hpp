//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/machine.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/exec/coro/async.hpp>
#include <sl/exec/coro/await.hpp>
#include <sl/exec/model/concept.hpp>
#include <sl/meta/func/function.hpp>
#include <sl/meta/match/overloaded.hpp>

namespace sl::http::v1::serialize {

template <typename WriterT>
concept Writer = requires(WriterT writer, std::span<const std::byte> buffer) {
    { writer.write(buffer) } -> exec::Signal<std::uint32_t, std::error_code>;
};

struct result_type {
    std::uint32_t written = 0;
    std::error_code ec{};

    bool combine(const result_type& other_r) {
        DEBUG_ASSERT(!ec);
        written += other_r.written;
        ec = other_r.ec;
        return !ec;
    };
};

struct config_type {
    std::size_t flush_threshold = 1024;
};

namespace detail {

using flush_type = meta::unique_function<exec::async<result_type>(std::span<const std::byte>)>;
exec::async<result_type> message(const message_type& m, flush_type flush, const config_type& config);

} // namespace detail

exec::async<result_type> message(const message_type& m, Writer auto& w, config_type config = {}) {
    using exec::operator co_await;
    const auto flush = [&w](std::span<const std::byte> b) -> exec::async<result_type> {
        const meta::result<std::uint32_t, std::error_code> partial_write_result = co_await w.write(b);
        if (!partial_write_result.has_value()) {
            co_return result_type{ .ec = partial_write_result.error() };
        }
        const auto partially_written = partial_write_result.value();
        if (partially_written == 0) {
            co_return result_type{ .ec = std::make_error_code(std::errc::io_error) };
        }
        co_return result_type{ .written = partially_written };
    };
    co_return co_await detail::message(m, flush, config);
}

namespace detail {

std::error_code verify_reason(const reason_type& reason);

exec::async<result_type> write_buffered(
    const config_type& config,
    flush_type& flush,
    v1::detail::remainder_buffer<>& remainder,
    std::span<const std::byte> b
);

exec::async<result_type> write_target(
    const target_type& target,
    meta::unique_function<exec::async<result_type>(std::string_view)> write_str
);

} // namespace detail
} // namespace sl::http::v1::serialize
