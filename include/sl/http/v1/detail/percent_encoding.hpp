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

bool percent_encode_is_unreserved(char c);
bool percent_encode_is_path_safe(char c);

void append_percent_encoded(std::string& result, char c);

// Encode string for use in URI path component
// Encodes all chars except: ALPHA / DIGIT / "-" / "." / "_" / "~" / ":" / "@" / "!" / "$" / "&" / "'" / "(" / ")" / "*"
// / "+" / "," / ";" / "=" Note: "/" is NOT encoded (path separator)
std::string percent_encode_path(std::string_view decoded);

// Encode string for use in query key or value
// Encodes all chars except: ALPHA / DIGIT / "-" / "." / "_" / "~"
// Space encoded as "+" per application/x-www-form-urlencoded
std::string percent_encode_query(std::string_view decoded);

} // namespace sl::http::v1::detail
