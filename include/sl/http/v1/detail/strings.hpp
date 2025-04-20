//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/enum/to_string.hpp>
#include <sl/meta/monad/maybe.hpp>
#include <sl/meta/monad/result.hpp>

#include <span>
#include <string_view>

namespace sl::http::v1::detail {

namespace tokens {

constexpr std::string_view CR = "\r";
constexpr std::string_view LF = "\n";
constexpr std::string_view CRLF = "\r\n";
constexpr std::string_view SP = " ";
constexpr std::string_view HTAB = "\t";
constexpr std::string_view HTTP = "HTTP";

} // namespace tokens

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer);
std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer);

std::string_view strip_prefix(std::string_view str, std::string_view prefix);
std::string_view strip_suffix(std::string_view str, std::string_view suffix);

struct find_ok {
    std::string_view value;
    std::size_t offset;
};

// TODO: visited_bytes
enum class find_err {
    NOT_FOUND,
    MAX_SIZE_EXCEEDED,
};

meta::result<find_ok, find_err>
    try_find_limited(std::string_view str_buffer, std::string_view delim, std::size_t maybe_max_size);

meta::result<find_ok, find_err> try_find_unlimited(std::string_view str_buffer, std::string_view delim);

meta::maybe<std::tuple<std::string_view, std::string_view>>
    try_find_split(std::string_view str_buffer, std::string_view delim);

template <typename EnumT>
constexpr std::size_t enum_max_str_length() {
    std::size_t max_str_length = 0;
    for (EnumT e{}; e != EnumT::ENUM_END; e = meta::next(e)) {
        std::string_view e_str = enum_to_str(e);
        max_str_length = std::max(max_str_length, e_str.size());
    }
    return max_str_length;
}

} // namespace sl::http::v1::detail
