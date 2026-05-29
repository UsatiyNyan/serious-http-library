//
// Created by usatiynyan.
//

#include "sl/http/v1/serialize/target.hpp"

#include <algorithm>
#include <array>
#include <variant>

namespace sl::http::v1 {

std::string serialize(const target_type& target) {
    return std::visit([](const auto& t) { return detail::serialize_impl(t); }, target);
}

namespace detail {

std::string serialize_impl(const origin_target_type& target) {
    std::string result;
    const std::size_t result_size = percent_encode::path_size(target.path) //
                                    + percent_encode::query_size(target.query);
    result.reserve(result_size);
    percent_encode::serialize_path(result, target.path);
    percent_encode::serialize_query(result, target.query);
    return result;
}
std::string serialize_impl(const absolute_target_type& target) {
    std::string result;
    constexpr std::string_view separator = "://";
    const std::size_t result_size = target.scheme.size() //
                                    + separator.size() //
                                    + target.authority.size() //
                                    + percent_encode::path_size(target.path) //
                                    + percent_encode::query_size(target.query);
    result.reserve(result_size);
    result += target.scheme;
    result += separator;
    result += target.authority;
    percent_encode::serialize_path(result, target.path);
    percent_encode::serialize_query(result, target.query);
    return result;
}
std::string serialize_impl(const authority_target_type& target) {
    std::string result;
    const std::size_t result_size = target.host.size() //
                                    + 1 // ':'
                                    + 5; // port max 65535
    result.reserve(result_size);
    result += target.host;
    result += ':';
    result += std::to_string(target.port);
    return result;
}
std::string serialize_impl(const asterisk_target_type&) { return "*"; }

bool percent_encode::is_unreserved(char c) {
    constexpr std::array allowed{ '-', '.', '_', '~' };
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
           || std::find(allowed.begin(), allowed.end(), c) != allowed.end();
}
bool percent_encode::is_path_safe(char c) {
    constexpr std::array allowed{ '/', ':', '@', '!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '=' };
    return is_unreserved(c) || std::find(allowed.begin(), allowed.end(), c) != allowed.end();
}

std::size_t percent_encode::path_size(std::string_view path) {
    std::size_t result = path.size();
    for (char c : path) {
        result += is_path_safe(c) ? 0 : 2; // should compile to branchless
    }
    return result;
}
std::size_t percent_encode::query_size(const query_params& query) {
    std::size_t size = query.size() * 2; // ('&' or '?') + '='
    for (const auto& [key, value] : query) {
        size += query_size(key) + query_size(value);
    }
    return size;
}
std::size_t percent_encode::query_size(std::string_view query_str) {
    std::size_t result = query_str.size();
    for (char c : query_str) {
        result += (is_unreserved(c) || c == ' ') ? 0 : 2; // should compile to branchless
    }
    return result;
}

void percent_encode::append(std::string& result, char c) {
    constexpr char hex_chars[] = "0123456789ABCDEF";
    result += '%';
    result += hex_chars[static_cast<std::uint8_t>(c) >> 4];
    result += hex_chars[static_cast<std::uint8_t>(c) & 0x0F];
}

void percent_encode::serialize_path(std::string& result, std::string_view path) {
    for (char c : path) {
        if (is_path_safe(c)) {
            result += c;
        } else {
            append(result, c);
        }
    }
}

void percent_encode::serialize_query(std::string& result, const query_params& query) {
    bool first = true;
    for (const auto& [key, value] : query) {
        const char c = std::exchange(first, false) ? '?' : '&';
        serialize_query(result, key);
        result += '=';
        serialize_query(result, value);
    }
}

void percent_encode::serialize_query(std::string& result, std::string_view query_str) {
    for (char c : query_str) {
        if (is_unreserved(c)) {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            append(result, c);
        }
    }
}

} // namespace detail
} // namespace sl::http::v1
