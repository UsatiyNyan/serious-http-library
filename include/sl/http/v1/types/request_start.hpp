//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <string>
#include <string_view>

namespace sl::http::v1 {

// https://www.rfc-editor.org/rfc/rfc9110#section-9
// method         = token
enum class method_type : std::uint8_t {
    GET, // Transfer a current representation of the target resource.
    HEAD, // Same as GET, but do not transfer the response content.
    POST, // Perform resource-specific processing on the request content.
    PUT, // Replace all current representations of the target resource with the request content.
    DELETE, // Remove all current representations of the target resource.
    CONNECT, // Establish a tunnel to the server identified by the target resource.
    OPTIONS, // Describe the communication options for the target resource.
    TRACE, // Perform a message loop-back test along the path to the target resource.
    ENUM_END
};

constexpr std::string_view enum_to_str(method_type e) {
    switch (e) {
    case method_type::GET:
        return "GET";
    case method_type::HEAD:
        return "HEAD";
    case method_type::POST:
        return "POST";
    case method_type::PUT:
        return "PUT";
    case method_type::DELETE:
        return "DELETE";
    case method_type::CONNECT:
        return "CONNECT";
    case method_type::OPTIONS:
        return "OPTIONS";
    case method_type::TRACE:
        return "TRACE";
    default:
        return {};
    }
}

// request-target = origin-form
//                / absolute-form
//                / authority-form
//                / asterisk-form
//
// origin-form    = absolute-path [ "?" query ]
// absolute-form  = absolute-URI
// authority-form = uri-host ":" port
// asterisk-form  = "*"
using target_type = std::string; // TODO: url

namespace detail {

meta::maybe<target_type> deserialize_request_target(std::string_view target_str);

} // namespace detail

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
