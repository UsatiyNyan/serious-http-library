//
// Created by usatiynyan.
//

#include "sl/http/v1/detail.hpp"

#include <bit>

namespace sl::http::v1::detail {

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

meta::maybe<std::tuple<std::string_view, std::string_view>>
    try_find_with_remainder(std::string_view str_buffer, std::string_view delim, std::size_t max_size) {
    const auto limited_str_buffer = str_buffer.substr(0, max_size);
    const std::size_t it = limited_str_buffer.find(delim);
    if (it == std::string_view::npos) {
        return meta::null;
    }
    return std::make_tuple(str_buffer.substr(0, it), str_buffer.substr(it + delim.size(), std::string_view::npos));
}

std::string_view buffer_byte_to_str(std::span<const std::byte> byte_buffer) {
    const auto char_buffer = std::bit_cast<std::span<const char>>(byte_buffer);
    return std::string_view{ char_buffer.data(), char_buffer.size() };
}

std::span<const std::byte> buffer_str_to_byte(std::string_view str_buffer) {
    const std::span<const char> char_buffer{ str_buffer.data(), str_buffer.size() };
    return std::bit_cast<std::span<const std::byte>>(char_buffer);
}

} // namespace sl::http::v1::detail
