//
// Created by usatiynyan.
//

#include "sl/http/v1/types/fields.hpp"
#include "sl/http/v1/detail.hpp"

namespace sl::http::v1::detail {

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

} // namespace sl::http::v1::detail
