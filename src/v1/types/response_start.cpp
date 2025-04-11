//
// Created by usatiynyan.
//

#include "sl/http/v1/types/response_start.hpp"
#include "sl/http/v1/detail.hpp"

#include <sl/meta/enum.hpp>

namespace sl::http::v1::detail {

// status-line = HTTP-version SP status-code SP [ reason-phrase ]
meta::maybe<response_start_type> deserialize_response_start(std::string_view start_line) {
    const auto version_result = try_find_with_remainder(start_line, tokens::SP);
    if (!version_result.has_value()) {
        return meta::null;
    }
    const auto& [version, version_remainder] = version_result.value();

    const auto status_result = try_find_with_remainder(version_remainder, tokens::SP);
    if (!status_result.has_value()) {
        return meta::null;
    }
    const auto& [status_str, status_remainder] = status_result.value();
    const auto status = meta::enum_from_str<status_type>(status_str);
    if (status == status_type::ENUM_END) {
        return meta::null;
    }

    const auto reason = status_remainder;

    return response_start_type{
        .version = version,
        .reason = reason,
        .status = status,
    };
}

} // namespace sl::http::v1::detail
