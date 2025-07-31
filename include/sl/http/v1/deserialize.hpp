//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/deserialize_request.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/meta/monad/result.hpp>

#include <cstddef>
#include <span>

namespace sl::http::v1::deserialize {

struct request_parser {
    using result_type = meta::result<request_message, status_type>;

    meta::maybe<result_type> process(std::span<const std::byte> byte_buffer) &;

private:
    using state_empty = detail::deserialize::request::state_empty;
    using state_complete = detail::deserialize::request::state_complete;

    detail::deserialize::request::machine m_{
        .state_variant{ state_empty{} },
        .remainder{},
    };
};

} // namespace sl::http::v1::deserialize
