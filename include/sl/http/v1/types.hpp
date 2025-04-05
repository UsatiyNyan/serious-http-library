//
// Created by usatiynyan.
// Data modelled according to RFC 9112.
//

#pragma once

#include <tsl/robin_map.h>

#include <span>
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

using body_type = std::span<const std::byte>;

struct request_start_type {
    std::string_view method;
    std::string_view target;
    std::string_view version;
};

struct request_type {
    request_start_type start;
    fields_type fields; // can be empty
    body_type body; // optional
};

struct response_start_type {
    std::string_view version;
    std::string_view status; // 3digit
    std::string_view reason; // optional
};

struct response_type {
    response_start_type start;
    fields_type fields; // can be empty
    body_type body; // optional
};

} // namespace sl::http::v1
