//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <tsl/robin_map.h>

#include <string_view>

namespace sl::http::v1 {

using fields_type = tsl::robin_map<
    /*Key=*/std::string,
    /*T=*/std::string>;

namespace detail {

meta::maybe<std::pair<std::string_view, std::string_view>> deserialize_field(std::string_view field_line);

} // namespace detail
} // namespace sl::http::v1
