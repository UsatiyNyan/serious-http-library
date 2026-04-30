//
// Created by usatiynyan.
//

#pragma once

#include <algorithm>
#include <sl/meta/enum/to_string.hpp>
#include <sl/meta/monad/result.hpp>

#include <span>
#include <string>
#include <string_view>

namespace sl::http::v1::detail {

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer);
std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer);

namespace tokens {

constexpr std::string_view SP = " ";
constexpr std::string_view COLON = ":";
constexpr std::string_view HTAB = "\t";
constexpr std::string_view CRLF = "\r\n";
constexpr std::string_view HTTP = "HTTP";

constexpr auto is_ws(char c) { return c == ' ' || c == '\t'; }

} // namespace tokens
} // namespace sl::http::v1::detail

namespace sl::http::v1::deserialize::detail {

std::string to_lowercase(std::string_view str);
bool is_lowercase(std::string_view str);

namespace tokens = v1::detail::tokens;
using v1::detail::buffer_byte_to_str;
using v1::detail::buffer_str_to_byte;

std::string_view strip_prefix(std::string_view str, std::string_view prefix);
std::string_view strip_suffix(std::string_view str, std::string_view suffix);

std::string_view strip_prefix(std::string_view str, char prefix);
std::string_view strip_suffix(std::string_view str, char suffix);

inline std::string_view strip_prefix_while(std::string_view str, std::predicate<char> auto pred) {
    auto it = std::find_if_not(str.begin(), str.end(), std::move(pred));
    const auto prefix_length = std::distance(str.begin(), it);
    str.remove_prefix(static_cast<std::size_t>(prefix_length));
    return str;
}
inline std::string_view strip_suffix_while(std::string_view str, std::predicate<char> auto pred) {
    auto it = std::find_if_not(str.rbegin(), str.rend(), std::move(pred));
    const auto suffix_length = std::distance(str.rbegin(), it);
    str.remove_suffix(static_cast<std::size_t>(suffix_length));
    return str;
}

struct find_ok {
    std::string_view value;
    std::size_t offset;
};

// TODO: visited_bytes
enum class find_err {
    NOT_FOUND,
    MAX_SIZE_EXCEEDED,
};

struct find_split_result {
    std::string_view head;
    meta::result<std::string_view, find_err> tail;
};

meta::result<find_ok, find_err> try_find_unlimited(std::string_view str_buffer, std::string_view delim);
meta::result<find_ok, find_err> try_find(std::string_view str_buffer, std::string_view delim, std::size_t max_size);

find_split_result try_find_split_unlimited(std::string_view str_buffer, std::string_view delim);
find_split_result try_find_split(std::string_view str_buffer, std::string_view delim, std::size_t max_size);

template <typename EnumT>
constexpr std::size_t enum_max_str_length() {
    std::size_t max_str_length = 0;
    for (EnumT e{}; e != EnumT::ENUM_END; e = meta::next(e)) {
        std::string_view e_str = enum_to_str(e);
        max_str_length = std::max(max_str_length, e_str.size());
    }
    return max_str_length;
}

} // namespace sl::http::v1::deserialize::detail
