//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types/status.hpp"

#include <sl/meta/monad/maybe.hpp>

#include <string_view>

namespace sl::http::v1 {

struct response_start_type {
    std::string_view version;
    std::string_view reason; // optional
    status_type status;
};

namespace detail {

meta::maybe<response_start_type> deserialize_response_start(std::string_view start_line);

} // namespace detail
} // namespace sl::http::v1
