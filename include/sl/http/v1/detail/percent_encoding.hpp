//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/monad/maybe.hpp>

#include <string>
#include <string_view>

namespace sl::http::v1::detail {

// Decode a single percent-encoded sequence (2 hex digits after %)
// Returns decoded char or null if invalid hex
meta::maybe<char> decode_percent_pair(char high, char low);

// Decode percent-encoded string
// Returns decoded string or null if any sequence is invalid
meta::maybe<std::string> percent_decode(std::string_view encoded);

// Decode query string component (also handles '+' as space)
meta::maybe<std::string> percent_decode_query(std::string_view encoded);

} // namespace sl::http::v1::detail
