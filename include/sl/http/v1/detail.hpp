//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <span>
#include <string_view>
#include <tuple>

namespace sl::http::v1::detail {

namespace tokens {

constexpr std::string_view CR = "\r";
constexpr std::string_view LF = "\n";
constexpr std::string_view CRLF = "\r\n";
constexpr std::string_view SP = " ";
constexpr std::string_view HTAB = "\t";
constexpr std::string_view HTTP = "HTTP";

} // namespace tokens

std::string_view strip_prefix(std::string_view str, std::string_view prefix);
std::string_view strip_suffix(std::string_view str, std::string_view suffix);

meta::maybe<std::tuple<std::string_view, std::string_view>> try_find_with_remainder(
    std::string_view str_buffer,
    std::string_view delim,
    std::size_t max_size = std::string_view::npos
);

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer);
std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer);

} // namespace sl::http::v1::detail
