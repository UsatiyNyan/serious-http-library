//
// Created by usatiynyan.
// Data modelled according to RFC 9112.
//

#pragma once

#include <span>
#include <string_view>
#include <vector>

namespace sl::http::v1 {

struct field_type {
    std::string_view name;
    std::string_view value;
};

using body_type = std::span<const std::byte>;

struct request_type {
    std::string_view method;
    std::string_view target;
    std::string_view version;

    std::vector<field_type> fields; // can be empty
    body_type body; // optional
};

struct response_type {
    std::string_view version;
    std::string_view status_code; // 3digit
    std::string_view reason; // optional

    std::vector<field_type> fields; // can be empty
    body_type body; // optional
};

} // namespace sl::http::v1
