//
// Created by usatiynyan.
//

#include "sl/http/v1/types/request_start.hpp"
#include "sl/http/v1/detail.hpp"

#include <sl/meta/enum.hpp>

namespace sl::http::v1::detail {

// request-line   = method SP request-target SP HTTP-version
meta::result<request_start_type, status_type> deserialize_request_start(std::string_view start_line) {
    const auto method_result = try_find_with_remainder(start_line, tokens::SP);
    if (!method_result.has_value()) {
        return meta::err(status_type::BAD_REQUEST);
    }
    const auto& [method_str, method_remainder] = method_result.value();
    const auto method = meta::enum_from_str<method_type>(method_str);
    if (method == method_type::ENUM_END) {
        return meta::err(status_type::BAD_REQUEST);
    }

    const auto target_result = try_find_with_remainder(method_remainder, tokens::SP);
    if (!target_result.has_value()) {
        return meta::null;
    }
    const auto& [target, target_remainder] = target_result.value();

    // TODO: proper validation
    const auto version = target_remainder;
    if (version.empty()) {
        return meta::null;
    }

    return request_start_type{
        .target = target,
        .version = version,
        .method = method,
    };
}

} // namespace sl::http::v1::detail
