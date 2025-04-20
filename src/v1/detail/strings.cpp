//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/strings.hpp"

#include <libassert/assert.hpp>

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

meta::result<find_ok, find_err>
    try_find_limited(std::string_view str_buffer, std::string_view delim, std::size_t max_size) {
    // avoiding integer overflows
    if (!DEBUG_ASSERT_VAL(max_size <= str_buffer.max_size() - delim.size())) {
        // TODO: .visited_bytes = 0,
        return meta::err(find_err::MAX_SIZE_EXCEEDED);
    }

    const std::size_t max_size_w_delim = max_size + delim.size();
    const std::size_t limited_str_buffer_size = std::min(max_size_w_delim, str_buffer.size());
    const auto limited_str_buffer = str_buffer.substr(0, limited_str_buffer_size);

    return try_find_unlimited(limited_str_buffer, delim).map_error([&](find_err err) {
        DEBUG_ASSERT(err == find_err::NOT_FOUND);
        const bool is_max_size_exceeded = max_size_w_delim <= str_buffer.size();
        // TODO: .visited_bytes = std::max(limited_str_buffer.size(), delim.size()) - delim.size(),
        if (is_max_size_exceeded) {
            return find_err::MAX_SIZE_EXCEEDED;
        }
        return err;
    });
}

meta::result<find_ok, find_err> try_find_unlimited(std::string_view str_buffer, std::string_view delim) {
    const std::size_t it = str_buffer.find(delim);

    if (it == std::string_view::npos) {
        return meta::err(find_err::NOT_FOUND);
    }

    return find_ok{
        .value = str_buffer.substr(0, it),
        .offset = it + delim.size(),
    };
}

meta::maybe<std::tuple<std::string_view, std::string_view>>
    try_find_split(std::string_view str_buffer, std::string_view delim) {
    if (str_buffer.empty()) {
        return meta::null;
    }

    const auto result = try_find_unlimited(str_buffer, delim);
    if (!result.has_value()) {
        return std::make_tuple(str_buffer, std::string_view{});
    }

    const auto value = result.value();
    return std::make_tuple(value.value, str_buffer.substr(value.offset));
}

} // namespace sl::http::v1::detail
