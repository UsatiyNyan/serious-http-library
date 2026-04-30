//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/percent_encoding.hpp"

namespace sl::http::v1::deserialize::detail {

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

} // namespace sl::http::v1::detail
