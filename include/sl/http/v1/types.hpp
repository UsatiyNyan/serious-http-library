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

using body_type = std::span<const std::byte>;

struct request_type {
    request_start_type start;
    fields_type fields; // can be empty
    body_type body; // optional
};

struct response_type {
    response_start_type start;
    fields_type fields; // can be empty
    body_type body; // optional
};

} // namespace sl::http::v1
