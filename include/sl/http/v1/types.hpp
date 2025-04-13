//
// Created by usatiynyan.
// Data modelled according to RFC 9112.
//

#pragma once

#include "sl/http/v1/types/request_start.hpp"
#include "sl/http/v1/types/response_start.hpp"

#include "sl/http/v1/types/fields.hpp"

#include <span>

namespace sl::http::v1 {

using target_type = std::string; // TODO: url

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

using body_type = std::span<const std::byte>;

struct request_message {
    request_start_type start;
    fields_type fields; // can be empty
    body_type body; // optional
};

struct response_message {
    response_start_type start;
    fields_type fields; // can be empty
    body_type body; // optional
};

} // namespace sl::http::v1
