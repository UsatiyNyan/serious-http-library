//
// Created by usatiynyan.
//

#include "sl/http/v1/types/request_start.hpp"

namespace sl::http::v1::detail {

meta::maybe<target_type> deserialize_request_target(std::string_view target_str) {
    // TODO
    return target_type{ target_str };
}

} // namespace sl::http::v1::detail
