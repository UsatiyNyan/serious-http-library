//
// Created by usatiynyan.
//

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sl::http::v1 {

// request-target = origin-form
//                / absolute-form
//                / authority-form
//                / asterisk-form
//
// origin-form    = absolute-path [ "?" query ]
// absolute-form  = absolute-URI
// authority-form = uri-host ":" port
// asterisk-form  = "*"

using query_params = std::vector<std::pair<std::string, std::string>>;

// origin-form = absolute-path [ "?" query ]
// e.g.: "/", "/path", "/path?key=value&foo=bar"
struct origin_target_type {
    std::string path;       // percent-decoded absolute-path
    query_params query;     // decoded query params
};

// absolute-form = absolute-URI
// e.g.: "http://example.com/path?query"
// TODO: 
//   scheme as enum
//   port as u16
struct absolute_target_type {
    std::string scheme;     // "http" or "https"
    std::string authority;  // host[:port]
    std::string path;       // percent-decoded path
    query_params query;
};

// authority-form = host ":" port
// Used only for CONNECT method
// e.g.: "example.com:443"
struct authority_target_type {
    std::string host;
    std::uint16_t port;
};

// asterisk-form = "*"
// Used only for server-wide OPTIONS request
struct asterisk_target_type {};

using target_type = std::variant<
    origin_target_type,
    absolute_target_type,
    authority_target_type,
    asterisk_target_type
>;

} // namespace sl::http::v1
