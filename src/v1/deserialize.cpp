//
// Created by usatiynyan.
//

#include "sl/http/v1/deserialize.hpp"

#include <bit>

namespace sl::http::v1 {

// HTTP-message   = start-line CRLF
//                 *( field-line CRLF )
//                 CRLF
//                 [ message-body ]

meta::maybe<request_type> deserialize_request(std::span<const std::byte> byte_buffer) {
    const std::string_view str_buffer = detail::buffer_byte_to_str(byte_buffer);

    const auto start_line_result = detail::try_find_with_remainder(str_buffer, tokens::CRLF);
    if (!start_line_result.has_value()) {
        return meta::null;
    }
    const auto& [start_line, start_line_remainder] = start_line_result.value();

    const auto start_result = detail::deserialize_request_start(start_line);
    if (!start_result.has_value()) {
        return meta::null;
    }

    auto fields_result = detail::deserialize_fields(start_line_remainder);
    if (!fields_result.has_value()) {
        return meta::null;
    }
    auto& [fields, fields_remainder] = fields_result.value();

    const auto body = detail::buffer_str_to_byte(fields_remainder);

    return request_type{
        .start = start_result.value(),
        .fields = std::move(fields),
        .body = body,
    };
}

meta::maybe<response_type> deserialize_response(std::span<const std::byte> byte_buffer) {
    const std::string_view str_buffer = detail::buffer_byte_to_str(byte_buffer);

    const auto start_line_result = detail::try_find_with_remainder(str_buffer, tokens::CRLF);
    if (!start_line_result.has_value()) {
        return meta::null;
    }
    const auto& [start_line, start_line_remainder] = start_line_result.value();

    const auto start_result = detail::deserialize_response_start(start_line);
    if (!start_result.has_value()) {
        return meta::null;
    }

    auto fields_result = detail::deserialize_fields(start_line_remainder);
    if (!fields_result.has_value()) {
        return meta::null;
    }
    auto& [fields, fields_remainder] = fields_result.value();

    const auto body = detail::buffer_str_to_byte(fields_remainder);

    return response_type{
        .start = start_result.value(),
        .fields = std::move(fields),
        .body = body,
    };
}

namespace detail {

// request-line   = method SP request-target SP HTTP-version
meta::maybe<request_start_type> deserialize_request_start(std::string_view start_line) {
    const auto method_result = try_find_with_remainder(start_line, tokens::SP);
    if (!method_result.has_value()) {
        return meta::null;
    }
    const auto& [method, method_remainder] = method_result.value();

    const auto target_result = try_find_with_remainder(method_remainder, tokens::SP);
    if (!target_result.has_value()) {
        return meta::null;
    }
    const auto& [target, target_remainder] = target_result.value();

    // TODO: proper validation
    const auto version = target_remainder;
    if (version.empty()) {
        return meta::null;
    }

    return request_start_type{
        .method = method,
        .target = target,
        .version = version,
    };
}

// status-line = HTTP-version SP status-code SP [ reason-phrase ]
meta::maybe<response_start_type> deserialize_response_start(std::string_view start_line) {
    const auto version_result = try_find_with_remainder(start_line, tokens::SP);
    if (!version_result.has_value()) {
        return meta::null;
    }
    const auto& [version, version_remainder] = version_result.value();

    const auto status_result = try_find_with_remainder(version_remainder, tokens::SP);
    if (!status_result.has_value()) {
        return meta::null;
    }
    const auto& [status, status_remainder] = status_result.value();

    const auto reason = status_remainder;

    return response_start_type{
        .version = version,
        .status = status,
        .reason = reason,
    };
}

// consumes last CRLF
meta::maybe<std::pair<fields_type, std::string_view>> deserialize_fields(std::string_view start_line_remainder) {
    fields_type fields;
    std::string_view fields_remainder = start_line_remainder;

    while (true) {
        const auto field_line_result = try_find_with_remainder(fields_remainder, tokens::CRLF);
        if (!field_line_result.has_value()) {
            return meta::null;
        }
        const auto& [field_line, field_line_remainder] = field_line_result.value();

        fields_remainder = field_line_remainder;
        if (field_line.empty()) { // detected last CRLF
            break;
        }

        const auto field_kv_result = deserialize_field(field_line);
        if (!field_kv_result.has_value()) {
            return meta::null;
        }
        const auto& [field_key, field_value] = field_kv_result.value();

        const auto [field_kv_it, field_kv_is_emplaced] = fields.try_emplace(field_key, field_value);
        if (std::ignore = field_kv_it; !field_kv_is_emplaced) {
            return meta::null;
        }
    }

    return std::make_pair(fields, fields_remainder);
}

// field-line   = field-name ":" OWS field-value OWS
// OWS = SP / HTAB
meta::maybe<std::pair<std::string_view, std::string_view>> deserialize_field(std::string_view field_line) {
    const auto field_kv_result = try_find_with_remainder(field_line, ":");
    if (!field_kv_result.has_value()) {
        return meta::null;
    }
    const auto& [field_key, field_value] = field_kv_result.value();
    const auto field_value_lstrip = strip_prefix(strip_prefix(field_value, tokens::SP), tokens::HTAB);
    const auto field_value_lrstrip = strip_suffix(strip_suffix(field_value_lstrip, tokens::SP), tokens::HTAB);
    return std::make_pair(field_key, field_value_lrstrip);
}

std::string_view strip_prefix(std::string_view str, std::string_view prefix) {
    const std::size_t prefix_length = str.starts_with(prefix) ? prefix.length() : 0;
    str.remove_prefix(prefix_length);
    return str;
}

std::string_view strip_suffix(std::string_view str, std::string_view suffix) {
    const std::size_t suffix_length = str.ends_with(suffix) ? suffix.length() : 0;
    str.remove_suffix(suffix_length);
    return str;
}

meta::maybe<std::tuple<std::string_view, std::string_view>>
    try_find_with_remainder(std::string_view str_buffer, std::string_view delim) {
    const std::size_t it = str_buffer.find(delim);
    if (it == std::string_view::npos) {
        return meta::null;
    }
    return std::make_tuple(str_buffer.substr(0, it), str_buffer.substr(it + delim.size(), std::string_view::npos));
}

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer) {
    const auto char_buffer = std::bit_cast<std::span<const char>>(byte_buffer);
    return std::string_view{ char_buffer.data(), char_buffer.size() };
}

std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer) {
    const std::span<const char> char_buffer{ str_buffer.data(), str_buffer.size() };
    return std::bit_cast<std::span<const std::byte>>(char_buffer);
}

} // namespace detail
} // namespace sl::http::v1
