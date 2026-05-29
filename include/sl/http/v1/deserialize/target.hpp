//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types/target.hpp"

#include <sl/meta/monad/maybe.hpp>

#include <string_view>

namespace sl::http::v1 {

// Main entry point - dispatches to specific form deserializers based on prefix
meta::maybe<target_type> deserialize_target(std::string_view target_str);

namespace detail {

// Parse origin-form: absolute-path [ "?" query ]
meta::maybe<origin_target_type> deserialize_origin_form(std::string_view target_str);

// Parse absolute-form: absolute-URI (http:// or https://)
meta::maybe<absolute_target_type> deserialize_absolute_form(std::string_view target_str);

// Parse authority-form: host ":" port
meta::maybe<authority_target_type> deserialize_authority_form(std::string_view target_str);

// Parse query string into key-value pairs
meta::maybe<query_params> deserialize_query_string(std::string_view query_str);

struct percent_decode {
    // Decode query string component (also handles '+' as space)
    static meta::maybe<std::string> query(std::string_view encoded);

    // Decode percent-encoded string
    // Returns decoded string or null if any sequence is invalid
    static meta::maybe<std::string> str(std::string_view encoded);

    // Decode a single percent-encoded sequence (2 hex digits after %)
    // Returns decoded char or null if invalid hex
    static meta::maybe<char> byte(char high, char low);
};

} // namespace detail
} // namespace sl::http::v1
