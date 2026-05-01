//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types/target.hpp"

#include <sl/meta/monad/maybe.hpp>

#include <string_view>

namespace sl::http::v1::deserialize {

// Main entry point - dispatches to specific form parsers based on prefix
meta::maybe<target_type> target(std::string_view target_str);

namespace detail {

// Parse origin-form: absolute-path [ "?" query ]
meta::maybe<origin_target_type> parse_origin_form(std::string_view target_str);

// Parse absolute-form: absolute-URI (http:// or https://)
meta::maybe<absolute_target_type> parse_absolute_form(std::string_view target_str);

// Parse authority-form: host ":" port
meta::maybe<authority_target_type> parse_authority_form(std::string_view target_str);

// Parse query string into key-value pairs
meta::maybe<query_params> parse_query_string(std::string_view query_str);

} // namespace detail
} // namespace sl::http::v1::deserialize
