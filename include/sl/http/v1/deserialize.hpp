//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types.hpp"

#include <sl/meta/monad/maybe.hpp>

#include <string_view>

namespace sl::http::v1 {

meta::result<request_type, status_type> deserialize_request(std::span<const std::byte> byte_buffer);
meta::result<request_type, status_type> deserialize_request(std::string_view str_buffer);

meta::maybe<response_type> deserialize_response(std::span<const std::byte> byte_buffer);
meta::maybe<response_type> deserialize_response(std::string_view str_buffer);

} // namespace sl::http::v1
