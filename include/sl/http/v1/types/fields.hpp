//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <tsl/robin_map.h>

#include <string>
#include <string_view>

namespace sl::http::v1 {

struct string_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

struct string_equal {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
};

using fields_type = tsl::robin_map<std::string, std::string, string_hash, string_equal>;

} // namespace sl::http::v1
