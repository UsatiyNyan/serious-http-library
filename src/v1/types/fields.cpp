//
// Created by usatiynyan.
//

#include "sl/http/v1/types/fields.hpp"
#include "sl/http/v1/detail.hpp"

namespace sl::http::v1::detail {

// field-line   = field-name ":" OWS field-value OWS
// OWS = SP / HTAB
meta::maybe<std::pair<std::string_view, std::string_view>> deserialize_field(std::string_view field_line) {
    const auto field_kv_result = try_find(field_line, ":");
    if (!field_kv_result.has_value()) {
        return meta::null;
    }
    const auto& [field_key, field_offset] = field_kv_result.value();
    const auto field_value = field_line.substr(field_offset);
    const auto field_value_lstrip = strip_prefix(strip_prefix(field_value, tokens::SP), tokens::HTAB);
    const auto field_value_lrstrip = strip_suffix(strip_suffix(field_value_lstrip, tokens::SP), tokens::HTAB);
    return std::make_pair(field_key, field_value_lrstrip);
}

} // namespace sl::http::v1::detail
