//
// Created by usatiynyan.
//

#include "sl/http/v1/serialize/message.hpp"
#include "sl/http/v1/detail/percent_encoding.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <utility>

namespace sl::http::v1::serialize::detail {

exec::async<result_type> message(const message_type& m, flush_type flush, const config_type& config) {
    namespace tokens = v1::detail::tokens;
    using exec::operator co_await;

    result_type r;
    if (auto* res = std::get_if<response_line_type>(&m.start_line); //
        res && !res->reason.empty()) {
        r.ec = detail::verify_reason(res->reason);
        if (r.ec) {
            co_return r;
        }
    }

    v1::detail::remainder_buffer<> remainder;
    const auto write_bin =
        [&config, &flush, &remainder](std::span<const std::byte> byte_buffer) -> exec::async<result_type> {
        return write_buffered(config, flush, remainder, byte_buffer);
    };
    const auto write_str = [&write_bin](std::string_view str) -> exec::async<result_type> {
        return write_bin(v1::detail::buffer_str_to_byte(str));
    };
    const auto write_enum = [&write_str](auto an_enum) -> exec::async<result_type> {
        return write_str(enum_to_str(an_enum));
    };

    // Serialize start line
    if (auto* req = std::get_if<request_line_type>(&m.start_line)) {
        r.combine(co_await write_enum(req->method)) //
            && r.combine(co_await write_str(tokens::SP)) //
            && r.combine(co_await write_target(req->target, write_str)) //
            && r.combine(co_await write_str(tokens::SP)) //
            && r.combine(co_await write_enum(req->version));
        if (r.ec) {
            co_return r;
        }
    } else {
        auto* res = std::get_if<response_line_type>(&m.start_line);
        DEBUG_ASSERT(res != nullptr);

        r.combine(co_await write_enum(res->version)) //
            && r.combine(co_await write_str(tokens::SP)) //
            && r.combine(co_await write_enum(res->status)) //
            && r.combine(co_await write_str(tokens::SP)) //
            && !res->reason.empty() && r.combine(co_await write_str(res->reason));
        if (r.ec) {
            co_return r;
        }
    }

    r.combine(co_await write_str(tokens::CRLF));
    if (r.ec) {
        co_return r;
    }

    for (const auto& [k, v] : m.fields) {
        r.combine(co_await write_str(k)) //
            && r.combine(co_await write_str(tokens::COLON)) //
            && r.combine(co_await write_str(v)) //
            && r.combine(co_await write_str(tokens::CRLF));
        if (r.ec) {
            co_return r;
        }
    }

    r.combine(co_await write_str(tokens::CRLF)) //
        && r.combine(co_await write_bin(m.body))
        && r.combine(co_await write_buffered(config_type{ .flush_threshold = 1 }, flush, remainder, {})); // final flush
    co_return r;
}

std::error_code verify_reason(const reason_type& reason) {
    // RFC 7230: reason-phrase = *( HTAB / SP / VCHAR / obs-text )
    // Must not contain CR or LF
    for (char c : reason) {
        if (c == '\r' || c == '\n') {
            return std::make_error_code(std::errc::invalid_argument);
        }
    }
    return {};
}

exec::async<result_type> write_buffered(
    const config_type& config,
    flush_type& flush,
    v1::detail::remainder_buffer<>& remainder,
    std::span<const std::byte> byte_buffer
) {
    using exec::operator co_await;

    result_type r;
    if (remainder.view().empty()) {
        while (!r.ec && byte_buffer.size() >= config.flush_threshold) {
            const result_type flush_r = co_await flush(byte_buffer);
            byte_buffer = byte_buffer.subspan(flush_r.written);
            r.written += flush_r.written;
            r.ec = flush_r.ec;
        }
    }

    if (!r.ec && !byte_buffer.empty()) {
        std::ignore = remainder.merge(byte_buffer);
    }

    while (!r.ec && remainder.view().size() >= config.flush_threshold) {
        const result_type flush_r = co_await flush(remainder.view());
        remainder.add_offset(flush_r.written);
        r.written += flush_r.written;
        r.ec = flush_r.ec;
    }
    co_return r;
}

exec::async<result_type> write_target(
    const target_type& target,
    meta::unique_function<exec::async<result_type>(std::string_view)> write_str
) {
    namespace tokens = v1::detail::tokens;
    using exec::operator co_await;

    const auto write_query = [&write_str](const query_params& query) -> exec::async<result_type> {
        result_type r;
        bool first = true;
        for (const auto& [key, value] : query) {
            r.combine(co_await write_str(std::exchange(first, false) ? "?" : "&"));
            if (r.ec) {
                co_return r;
            }
            const std::string encoded_key = percent_encode_query(key);
            const std::string encoded_value = percent_encode_query(value);
            r.combine(co_await write_str(encoded_key)) //
                && r.combine(co_await write_str("=")) //
                && r.combine(co_await write_str(encoded_value));
            if (r.ec) {
                co_return r;
            }
        }
        co_return r;
    };

    switch (target.index()) {
    case 0: {
        origin_target_type origin_target = std::get<0>(target);
        result_type r;
        const std::string encoded_path = percent_encode_path(origin_target.path);
        r.combine(co_await write_str(encoded_path)) //
            && r.combine(co_await write_query(origin_target.query));
        co_return r;
    }
    case 1: {
        absolute_target_type absolute_target = std::get<1>(target);
        result_type r;
        std::string encoded_path = percent_encode_path(absolute_target.path);
        r.combine(co_await write_str(absolute_target.scheme)) //
            && r.combine(co_await write_str("://")) //
            && r.combine(co_await write_str(absolute_target.authority)) //
            && r.combine(co_await write_str(encoded_path)) //
            && r.combine(co_await write_query(absolute_target.query));
        co_return r;
    }
    case 2: {
        authority_target_type authority_target = std::get<2>(target);
        result_type r;
        std::string port_str = std::to_string(authority_target.port);
        r.combine(co_await write_str(authority_target.host)) //
            && r.combine(co_await write_str(tokens::COLON)) //
            && r.combine(co_await write_str(port_str));
        co_return r;
    }
    case 3: {
        [[maybe_unused]] asterisk_target_type asterisk_target = std::get<3>(target);
        co_return co_await write_str("*");
    }
    default:
        std::unreachable();
    }
}

} // namespace sl::http::v1::serialize::detail
