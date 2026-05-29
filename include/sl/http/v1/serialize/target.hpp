//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types/target.hpp"

namespace sl::http::v1 {

std::string serialize(const target_type& target);

namespace detail {

std::string serialize_impl(const origin_target_type& target);
std::string serialize_impl(const absolute_target_type& target);
std::string serialize_impl(const authority_target_type& target);
std::string serialize_impl(const asterisk_target_type&);

struct percent_encode {
    static bool is_unreserved(char c);
    static bool is_path_safe(char c);

    static std::size_t path_size(std::string_view path);
    static std::size_t query_size(const query_params& query);
    static std::size_t query_size(std::string_view query_str);

    static void append(std::string& result, char c);

    // Encode string for use in URI path component
    // Encodes all chars except: ALPHA / DIGIT / "-" / "." / "_" / "~" / ":" / "@" / "!" / "$" / "&" / "'" / "(" / ")" /
    // "*" / "+" / "," / ";" / "=" Note: "/" is NOT encoded (path separator)
    static void serialize_path(std::string& result, std::string_view path);

    // Encode string for use in query key or value
    // Encodes all chars except: ALPHA / DIGIT / "-" / "." / "_" / "~"
    // Space encoded as "+" per application/x-www-form-urlencoded
    static void serialize_query(std::string& result, const query_params& query);
    static void serialize_query(std::string& result, std::string_view query_str);
};

} // namespace detail
} // namespace sl::http::v1
