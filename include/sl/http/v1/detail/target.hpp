//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types/target.hpp"

#include <sl/meta/monad/maybe.hpp>

namespace sl::http::v1::detail::deserialize {

inline meta::maybe<target_type> target(std::string_view target_str) {
    // TODO
    return target_type{ target_str };
}

} // namespace sl::http::v1::detail::deserialize
