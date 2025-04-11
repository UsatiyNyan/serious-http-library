//
// Created by usatiynyan.
//
// HTTP-message   = start-line CRLF
//                 *( field-line CRLF )
//                 CRLF
//                 [ message-body ]
//

#include "sl/http/v1/deserialize.hpp"

#include "sl/http/v1/detail.hpp"

namespace sl::http::v1 {

meta::result<request_type, status_type> deserialize_request(std::span<const std::byte> byte_buffer) {
    return deserialize_request(detail::buffer_byte_to_str(byte_buffer));
}

meta::result<request_type, status_type> deserialize_request(std::string_view str_buffer) {
    const auto start_line_result = detail::try_find_with_remainder(str_buffer, detail::tokens::CRLF);
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
    return deserialize_response(detail::buffer_byte_to_str(byte_buffer));
}

meta::maybe<response_type> deserialize_response(std::string_view str_buffer) {
    const auto start_line_result = detail::try_find_with_remainder(str_buffer, detail::tokens::CRLF);
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

} // namespace sl::http::v1
