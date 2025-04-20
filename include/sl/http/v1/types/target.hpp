//
// Created by usatiynyan.
//

#pragma once

#include <string>

namespace sl::http::v1 {

// request-target = origin-form
//                / absolute-form
//                / authority-form
//                / asterisk-form
//
// origin-form    = absolute-path [ "?" query ]
// absolute-form  = absolute-URI
// authority-form = uri-host ":" port
// asterisk-form  = "*"
using target_type = std::string; // TODO: url

} // namespace sl::http::v1
