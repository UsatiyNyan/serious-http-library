//
// Created by usatiynyan.
//

#include "sl/http/v1/detail.hpp"

#include <bit>

namespace sl::http::v1::detail {

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer) {
    const auto char_buffer = std::bit_cast<std::span<const char>>(byte_buffer);
    return std::string_view{ char_buffer.data(), char_buffer.size() };
}

std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer) {
    const std::span<const char> char_buffer{ str_buffer.data(), str_buffer.size() };
    return std::bit_cast<std::span<const std::byte>>(char_buffer);
}

std::string_view strip_prefix(std::string_view str, std::string_view prefix) {
    const std::size_t prefix_length = str.starts_with(prefix) ? prefix.length() : 0;
    str.remove_prefix(prefix_length);
    return str;
}

std::string_view strip_suffix(std::string_view str, std::string_view suffix) {
    const std::size_t suffix_length = str.ends_with(suffix) ? suffix.length() : 0;
    str.remove_suffix(suffix_length);
    return str;
}

meta::result<find_ok, find_err> try_find(std::string_view str_buffer, std::string_view delim, std::size_t max_size) {
    const std::size_t it = str_buffer.substr(0, max_size).find(delim);
    if (it == std::string_view::npos) {
        const find_err err = max_size > str_buffer.size() ? find_err::NOT_FOUND : find_err::MAX_SIZE_EXCEEDED;
        return meta::err(err);
    }
    return find_ok{
        .value = str_buffer.substr(0, it),
        .offset = it + delim.size(),
    };
}

} // namespace sl::http::v1::detail
