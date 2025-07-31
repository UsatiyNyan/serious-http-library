//
// Created by usatiynyan.
//
// HTTP-message   = start-line CRLF
//                 *( field-line CRLF )
//                 CRLF
//                 [ message-body ]
//
// start-line     = request-line
// request-line   = method SP request-target SP HTTP-version
//

#include "sl/http/v1/detail/deserialize_request.hpp"
#include "sl/http/v1/detail/strings.hpp"
#include "sl/http/v1/detail/target.hpp"

#include <sl/meta/enum/from_string.hpp>

#include <libassert/assert.hpp>

#include <charconv>
#include <utility>

namespace sl::http::v1::detail::deserialize::request {

// method SP
process_result state_empty::process(std::span<const std::byte> byte_buffer) && {
    constexpr std::size_t method_max_length = enum_max_str_length<method_type>();
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto method_result = try_find_limited(str_buffer, tokens::SP, method_max_length);
    if (!method_result.has_value()) {
        const auto& method_err = method_result.error();
        if (method_err == find_err::MAX_SIZE_EXCEEDED) {
            return state_complete::process_err(status_type::NOT_IMPLEMENTED);
        }
        DEBUG_ASSERT(method_err == find_err::NOT_FOUND);
        return process_result::err(std::move(*this));
    }

    const auto& [method_str, method_offset] = method_result.value();
    const auto method = meta::enum_from_str<method_type>(method_str);
    if (method == method_type::ENUM_END) {
        return state_complete::process_err(status_type::BAD_REQUEST);
    }

    return process_result::ok(state_method{ std::move(*this), method }, method_offset);
}

// request-target SP
process_result state_method::process(std::span<const std::byte> byte_buffer) && {
    // recommended as per https://www.rfc-editor.org/rfc/rfc9112.html#name-request-line
    constexpr std::size_t target_max_length = 8000;
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto target_result = try_find_limited(str_buffer, tokens::SP, target_max_length);
    if (!target_result.has_value()) {
        const auto& target_err = target_result.error();
        if (target_err == find_err::MAX_SIZE_EXCEEDED) {
            return state_complete::process_err(status_type::URI_TOO_LONG);
        }
        DEBUG_ASSERT(target_err == find_err::NOT_FOUND);
        return process_result::err(std::move(*this));
    }
    const auto& [target_str, target_offset] = target_result.value();

    auto maybe_target = deserialize::target(target_str);
    if (!maybe_target.has_value()) {
        return state_complete::process_err(status_type::BAD_REQUEST);
    }

    return process_result::ok(state_target{ std::move(*this), std::move(maybe_target).value() }, target_offset);
}

// HTTP-version CRLF
process_result state_target::process(std::span<const std::byte> byte_buffer) && {
    constexpr std::size_t version_max_length = enum_max_str_length<version_type>();
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto version_result = try_find_limited(str_buffer, tokens::CRLF, version_max_length);
    if (!version_result.has_value()) {
        const auto& version_err = version_result.error();
        if (version_err == find_err::MAX_SIZE_EXCEEDED) {
            return state_complete::process_err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(version_err == find_err::NOT_FOUND);
        return process_result::err(std::move(*this));
    }

    const auto& [version_str, version_offset] = version_result.value();
    const auto version = meta::enum_from_str<version_type>(version_str);
    if (version == version_type::ENUM_END) {
        return state_complete::process_err(status_type::BAD_REQUEST);
    }

    return process_result::ok(state_version{ std::move(*this), version }, version_offset);
}

process_result state_version::process(std::span<const std::byte> byte_buffer [[maybe_unused]]) && {
    return process_result::ok(state_fields{ std::move(*this) }, 0);
}

// *( field-line CRLF ) CRLF
// field-line   = field-name ":" OWS field-value OWS
// OWS = SP / HTAB
process_result state_fields::process(std::span<const std::byte> byte_buffer) && {
    constexpr std::size_t max_fields_size = 80 * 1024; // 80KiB
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto field_line_result = try_find_limited(str_buffer, tokens::CRLF, max_fields_size - fields_byte_size_total);
    if (!field_line_result.has_value()) {
        const auto& field_line_err = field_line_result.error();
        if (field_line_err == find_err::MAX_SIZE_EXCEEDED) {
            return state_complete::process_err(status_type::CONTENT_TOO_LARGE);
        }
        DEBUG_ASSERT(field_line_err == find_err::NOT_FOUND);
        return process_result::err(std::move(*this));
    }
    const auto& [field_line, field_line_offset] = field_line_result.value();

    if (field_line.empty()) { // detected last CRLF
        return process_result::ok(std::move(*this).finalize(), field_line_offset);
    }

    // not limiting by max_size since field_line_result is already limited
    const auto field_kv_result = try_find_unlimited(field_line, ":");
    if (!field_kv_result.has_value()) {
        return state_complete::process_err(status_type::BAD_REQUEST);
    }
    const auto& [field_key, field_offset] = field_kv_result.value();
    constexpr auto strip = [](std::string_view x) {
        x = strip_prefix(x, tokens::SP);
        x = strip_prefix(x, tokens::HTAB);
        x = strip_suffix(x, tokens::SP);
        x = strip_suffix(x, tokens::HTAB);
        return x;
    };
    const auto field_value = strip(field_line.substr(field_offset));

    const auto [field_kv_it, field_kv_is_emplaced] = fields.try_emplace(std::string{ field_key }, field_value);
    std::ignore = field_kv_it;
    if (!field_kv_is_emplaced) {
        return state_complete::process_err(status_type::BAD_REQUEST);
    }

    fields_byte_size_total += field_line_offset;
    return process_result::ok(std::move(*this), field_line_offset);
}

state_variant state_fields::finalize() && {
    const auto find_field = [this](auto k) -> meta::maybe<std::string_view> {
        auto it = fields.find(k); // can avoid string creation
        if (it == fields.end()) {
            return meta::null;
        }
        return std::string_view{ it.value() };
    };

    const auto maybe_transfer_encodings = find_field("Transfer-Encoding");
    const auto maybe_content_length_str = find_field("Content-Length");

    const bool is_chunked = maybe_transfer_encodings
                                .map([](std::string_view transfer_encodings) {
                                    while (const auto maybe_next = try_find_split(transfer_encodings, ", ")) {
                                        const auto [transfer_encoding, remainder] = maybe_next.value();
                                        if (transfer_encoding == "chunked") {
                                            return true;
                                        }
                                        transfer_encodings = remainder;
                                    }
                                    return false;
                                })
                                .value_or(false);

    if (is_chunked) {
        if (maybe_content_length_str.has_value()) {
            // TODO: or is it?
            return state_complete{ meta::err(status_type::BAD_REQUEST) };
        }

        return state_chunked_body{ std::move(*this) };
    }

    const auto maybe_content_length =
        maybe_content_length_str.and_then([](std::string_view content_length_str) -> meta::maybe<std::size_t> {
            std::size_t content_length = 0;
            const auto conv_result =
                std::from_chars(content_length_str.begin(), content_length_str.end(), content_length);
            if (conv_result.ec != std::error_code{} || conv_result.ptr != content_length_str.end()) {
                return meta::null;
            }
            return content_length;
        });

    if (!maybe_content_length.has_value()) {
        return state_complete{ meta::err(status_type::BAD_REQUEST) };
    }
    const std::size_t content_length = maybe_content_length.value();

    return state_body{ std::move(*this), content_length };
}

process_result state_body::process(std::span<const std::byte> byte_buffer) && {
    ASSERT(content_length > body.size());

    const std::size_t content_length_left = content_length - body.size();
    const std::size_t limited_byte_buffer_size = std::min(content_length_left, byte_buffer.size());
    const auto limited_byte_buffer = byte_buffer.subspan(0, limited_byte_buffer_size);
    body.insert(body.end(), limited_byte_buffer.begin(), limited_byte_buffer.end());

    if (body.size() < content_length) {
        return process_result::ok(std::move(*this), limited_byte_buffer_size);
    }

    ASSERT(body.size() == content_length);
    return process_result::ok(state_complete{ std::move(*this) }, limited_byte_buffer_size);
}

process_result state_chunked_body::process(std::span<const std::byte> byte_buffer) && {
    // TODO
    return state_complete::process_err(status_type::BAD_REQUEST);
}

process_result process(state_trailing_fields state, std::span<const std::byte> byte_buffer) {
    // TODO
    return state_complete::process_err(status_type::BAD_REQUEST);
}

process_result state_complete::process(std::span<const std::byte> byte_buffer [[maybe_unused]]) && {
    return process_result::err(std::move(*this));
}

} // namespace sl::http::v1::detail::deserialize::request
