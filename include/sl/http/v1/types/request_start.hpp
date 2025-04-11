//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types/method.hpp"
#include "sl/http/v1/types/status.hpp"

#include <sl/meta/monad/result.hpp>

#include <string_view>

namespace sl::http::v1 {

struct request_start_type {
    std::string_view target;
    std::string_view version;
    method_type method;
};

namespace detail {

constexpr std::size_t max_request_start_length = 8000;

meta::result<request_start_type, status_type> deserialize_request_start(std::string_view start_line);

} // namespace detail
} // namespace sl::http::v1
