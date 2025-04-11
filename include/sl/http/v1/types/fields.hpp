//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <tsl/robin_map.h>

#include <string_view>

namespace sl::http::v1 {

using fields_type = tsl::robin_map<
    /* Key = */ std::string_view,
    /* T = */ std::string_view,
    /* Hash =  */ std::hash<std::string_view>,
    /* KeyEqual = */ std::equal_to<std::string_view>,
    /* Allocator = */ std::allocator<std::pair<std::string_view, std::string_view>>,
    /* StoreHash = */ true, // <-- this is changed, since a cache miss might occur due to keys being non-owning
    /* GrowthPolicy = */ tsl::rh::power_of_two_growth_policy<2>>;

namespace detail {

meta::maybe<std::pair<fields_type, std::string_view>> deserialize_fields(std::string_view start_line_remainder);
meta::maybe<std::pair<std::string_view, std::string_view>> deserialize_field(std::string_view field_line);

} // namespace detail
} // namespace sl::http::v1
