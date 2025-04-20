//
// Created by usatiynyan.
// https://www.rfc-editor.org/rfc/rfc9110#section-15
//

#pragma once

#include <string_view>

namespace sl::http::v1 {

enum class status_type : std::uint16_t {
    // INFORMATIONAL
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    // SUCCESSFUL
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NON_AUTHORITATIVE_INFORMATION = 203,
    NO_CONTENT = 204,
    RESET_CONTENT = 205,
    PARTIAL_CONTENT = 206,
    // REDIRECTION
    MUTLIPLE_CHOICES = 300,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    USE_PROXY = 305,
    TEMPORARILY_RESTRICT = 307,
    PERMANENT_REDIRECT = 308,
    // CLIENT ERROR
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    PAYMENT_REQUIRED = 402,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    PROXY_AUTHENTICATION_REQUIRED = 407,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PRECONDITION_FAILED = 412,
    CONTENT_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    MISDIRECTED_REQUEST = 421,
    UNPROCESSABLE_CONTENT = 422,
    UPGRADE_REQUIRED = 426,
    // SERVER ERROR
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_NOT_SUPPORTED = 505,
    //
    ENUM_END
};

constexpr std::string_view enum_to_str(status_type e) {
    switch (e) {
    case status_type::CONTINUE:
        return "100";
    case status_type::SWITCHING_PROTOCOLS:
        return "101";
    case status_type::OK:
        return "200";
    case status_type::CREATED:
        return "201";
    case status_type::ACCEPTED:
        return "202";
    case status_type::NON_AUTHORITATIVE_INFORMATION:
        return "203";
    case status_type::NO_CONTENT:
        return "204";
    case status_type::RESET_CONTENT:
        return "205";
    case status_type::PARTIAL_CONTENT:
        return "206";
    case status_type::MUTLIPLE_CHOICES:
        return "300";
    case status_type::MOVED_PERMANENTLY:
        return "301";
    case status_type::FOUND:
        return "302";
    case status_type::SEE_OTHER:
        return "303";
    case status_type::NOT_MODIFIED:
        return "304";
    case status_type::USE_PROXY:
        return "305";
    case status_type::TEMPORARILY_RESTRICT:
        return "307";
    case status_type::PERMANENT_REDIRECT:
        return "308";
    case status_type::BAD_REQUEST:
        return "400";
    case status_type::UNAUTHORIZED:
        return "401";
    case status_type::PAYMENT_REQUIRED:
        return "402";
    case status_type::FORBIDDEN:
        return "403";
    case status_type::NOT_FOUND:
        return "404";
    case status_type::METHOD_NOT_ALLOWED:
        return "405";
    case status_type::NOT_ACCEPTABLE:
        return "406";
    case status_type::PROXY_AUTHENTICATION_REQUIRED:
        return "407";
    case status_type::REQUEST_TIMEOUT:
        return "408";
    case status_type::CONFLICT:
        return "409";
    case status_type::GONE:
        return "410";
    case status_type::LENGTH_REQUIRED:
        return "411";
    case status_type::PRECONDITION_FAILED:
        return "412";
    case status_type::CONTENT_TOO_LARGE:
        return "413";
    case status_type::URI_TOO_LONG:
        return "414";
    case status_type::UNSUPPORTED_MEDIA_TYPE:
        return "415";
    case status_type::RANGE_NOT_SATISFIABLE:
        return "416";
    case status_type::EXPECTATION_FAILED:
        return "417";
    case status_type::MISDIRECTED_REQUEST:
        return "421";
    case status_type::UNPROCESSABLE_CONTENT:
        return "422";
    case status_type::UPGRADE_REQUIRED:
        return "426";
    case status_type::INTERNAL_SERVER_ERROR:
        return "500";
    case status_type::NOT_IMPLEMENTED:
        return "501";
    case status_type::BAD_GATEWAY:
        return "502";
    case status_type::SERVICE_UNAVAILABLE:
        return "503";
    case status_type::GATEWAY_TIMEOUT:
        return "504";
    case status_type::HTTP_VERSION_NOT_SUPPORTED:
        return "505";
    default:
        return {};
    }
}

} // namespace sl::http::v1
