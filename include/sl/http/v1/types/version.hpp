//
// Created by usatiynyan.
//

#pragma once

#include <string_view>

namespace sl::http::v1 {

// HTTP-version  = HTTP-name "/" DIGIT "." DIGIT
// HTTP-name     = %s"HTTP"
enum class version_type {
    HTTPv1_0,
    HTTPv1_1,
    ENUM_END,
};

constexpr std::string_view enum_to_str(version_type e) {
    switch (e) {
    case version_type::HTTPv1_0:
        return "HTTP/1.0";
    case version_type::HTTPv1_1:
        return "HTTP/1.1";
    default:
        return {};
    }
}

} // namespace sl::http::v1
