//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types.hpp"

#include <sl/meta/monad/maybe.hpp>

#include <string_view>
#include <tuple>

namespace sl::http::v1 {

meta::maybe<request_type> deserialize_request(std::span<const std::byte> byte_buffer);
meta::maybe<response_type> deserialize_response(std::span<const std::byte> byte_buffer);

namespace tokens {

constexpr std::string_view CR = "\r";
constexpr std::string_view LF = "\n";
constexpr std::string_view CRLF = "\r\n";
constexpr std::string_view SP = " ";
constexpr std::string_view HTAB = "\t";
constexpr std::string_view HTTP = "HTTP";

} // namespace tokens

namespace detail {

meta::maybe<request_start_type> deserialize_request_start(std::string_view start_line);
meta::maybe<response_start_type> deserialize_response_start(std::string_view start_line);

meta::maybe<std::pair<fields_type, std::string_view>> deserialize_fields(std::string_view start_line_remainder);
meta::maybe<std::pair<std::string_view, std::string_view>> deserialize_field(std::string_view field_line);

std::string_view strip_prefix(std::string_view str, std::string_view prefix);
std::string_view strip_suffix(std::string_view str, std::string_view suffix);

meta::maybe<std::tuple<std::string_view, std::string_view>>
    try_find_with_remainder(std::string_view str_buffer, std::string_view delim);

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer);
std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer);

} // namespace detail
} // namespace sl::http::v1
