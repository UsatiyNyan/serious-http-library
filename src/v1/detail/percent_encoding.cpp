//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/percent_encoding.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace sl::http::v1::detail {

meta::maybe<char> decode_percent_pair(char high, char low) {
    auto hex_to_val = [](char c) -> meta::maybe<int> {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return meta::null;
    };

    auto high_val = hex_to_val(high);
    auto low_val = hex_to_val(low);

    if (!high_val.has_value() || !low_val.has_value()) {
        return meta::null;
    }

    return static_cast<char>((high_val.value() << 4) | low_val.value());
}

meta::maybe<std::string> percent_decode(std::string_view encoded) {
    std::string result;
    result.reserve(encoded.size());

    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.size()) {
                return meta::null; // incomplete percent encoding
            }
            auto maybe_char = decode_percent_pair(encoded[i + 1], encoded[i + 2]);
            if (!maybe_char.has_value()) {
                return meta::null;
            }
            result += maybe_char.value();
            i += 2;
        } else {
            result += encoded[i];
        }
    }

    return result;
}

meta::maybe<std::string> percent_decode_query(std::string_view encoded) {
    std::string result;
    result.reserve(encoded.size());

    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.size()) {
                return meta::null;
            }
            auto maybe_char = decode_percent_pair(encoded[i + 1], encoded[i + 2]);
            if (!maybe_char.has_value()) {
                return meta::null;
            }
            result += maybe_char.value();
            i += 2;
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }

    return result;
}

bool percent_encode_is_unreserved(char c) {
    constexpr std::array allowed{ '-', '.', '_', '~' };
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
           || std::find(allowed.begin(), allowed.end(), c) != allowed.end();
}

bool percent_encode_is_path_safe(char c) {
    constexpr std::array allowed{ '/', ':', '@', '!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '=' };
    return percent_encode_is_unreserved(c) || std::find(allowed.begin(), allowed.end(), c) != allowed.end();
}

void append_percent_encoded(std::string& result, char c) {
    constexpr char hex_chars[] = "0123456789ABCDEF";
    result += '%';
    result += hex_chars[static_cast<std::uint8_t>(c) >> 4];
    result += hex_chars[static_cast<std::uint8_t>(c) & 0x0F];
}

std::string percent_encode_path(std::string_view decoded) {
    std::string result;
    result.reserve(decoded.size());

    for (char c : decoded) {
        if (percent_encode_is_path_safe(c)) {
            result += c;
        } else {
            append_percent_encoded(result, c);
        }
    }

    return result;
}

std::string percent_encode_query(std::string_view decoded) {
    std::string result;
    result.reserve(decoded.size());

    for (char c : decoded) {
        if (percent_encode_is_unreserved(c)) {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            append_percent_encoded(result, c);
        }
    }

    return result;
}

} // namespace sl::http::v1::detail
