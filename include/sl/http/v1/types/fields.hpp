//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <tsl/robin_map.h>

#include <string>

namespace sl::http::v1 {

using fields_type = tsl::robin_map<std::string, std::string>;

} // namespace sl::http::v1
