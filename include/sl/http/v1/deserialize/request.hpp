//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/machine.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/exec/coro/async.hpp>
#include <sl/exec/coro/async_gen.hpp>
#include <sl/exec/model/concept.hpp>

#include <sl/meta/monad/result.hpp>
#include <sl/meta/type/unit.hpp>

#include <cstddef>
#include <span>

namespace sl::http::v1::deserialize {

template <typename T>
using io_result = meta::result<T, std::error_code>;

using request_result = meta::result<request_message, status_type>;

struct request_config {
    std::size_t max_body_size = 1 * 1024 * 1024; // 1 MiB default
};

struct request_chunk {
    const request_message& message;
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};

exec::async_gen<request_chunk, io_result<request_result>>
    request(exec::async_gen<std::span<const std::byte>, std::error_code> input, request_config config = {});

namespace detail {

struct request_state_empty {};
struct request_state_method {};
struct request_state_target {};
struct request_state_version {};
struct request_state_fields {
    std::size_t consumed_bytes = 0;
};
struct request_state_body {
    std::size_t content_length = 0;
};

struct request_state_chunked_body_empty {};
struct request_state_chunked_body_line {
    std::string chunk_ext;
    std::uint32_t chunk_size = 0;
};
struct request_state_chunked_body_complete {
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};
using request_state_chunked_body = std::variant< //
    request_state_chunked_body_empty,
    request_state_chunked_body_line,
    request_state_chunked_body_complete>;

struct request_state_trailing_fields {
    std::size_t consumed_bytes = 0;
};
struct request_state_complete {};

using request_state = std::variant<
    request_state_empty,
    request_state_method,
    request_state_target,
    request_state_version,
    request_state_fields,
    request_state_body,
    request_state_chunked_body,
    request_state_trailing_fields,
    request_state_complete>;

inline parse_result<meta::result<request_state, status_type>> make_parse_stop(status_type status) {
    return parse_result<meta::result<request_state, status_type>>::stop(meta::err(status));
}

inline parse_result<meta::result<request_state, status_type>> make_parse_stop(request_state state) {
    return parse_result<meta::result<request_state, status_type>>::stop(state);
}

inline parse_result<meta::result<request_state, status_type>> make_parse_more(request_state state, std::size_t offset) {
    return parse_result<meta::result<request_state, status_type>>::more(state, offset);
}

parse_result<meta::result<request_state, status_type>>
    parse_request(request_message& output, request_state state, std::span<const std::byte> byte_buffer, std::size_t max_body_size);

parse_result<meta::result<request_state, status_type>>
    parse_request_part(request_message& output, request_state_empty state, std::string_view buffer);

parse_result<meta::result<request_state, status_type>>
    parse_request_part(request_message& output, request_state_method state, std::string_view buffer);

parse_result<meta::result<request_state, status_type>>
    parse_request_part(request_message& output, request_state_target state, std::string_view buffer);

parse_result<meta::result<request_state, status_type>> parse_request_part(
    request_message& output,
    std::variant<request_state_fields, request_state_trailing_fields> state,
    std::string_view buffer,
    std::size_t max_body_size
);
meta::result<request_state, status_type>
    parse_fields_finalize(request_message& output, std::size_t fields_consumed_bytes, std::size_t max_body_size);

parse_result<meta::result<request_state, status_type>>
    parse_request_part(request_message& output, request_state_body state, std::span<const std::byte> buffer);

inline parse_result<meta::result<request_state_chunked_body, status_type>>
    make_parse_chunk_stop(request_state_chunked_body state) {
    return parse_result<meta::result<request_state_chunked_body, status_type>>::stop(state);
};

inline parse_result<meta::result<request_state_chunked_body, status_type>> make_parse_chunk_stop(status_type status) {
    return parse_result<meta::result<request_state_chunked_body, status_type>>::stop(meta::err(status));
};

inline parse_result<meta::result<request_state_chunked_body, status_type>>
    make_parse_chunk_more(request_state_chunked_body state, std::size_t offset) {
    return parse_result<meta::result<request_state_chunked_body, status_type>>::more(state, offset);
};

parse_result<meta::result<request_state_chunked_body, status_type>>
    parse_request_chunk(request_message& output, request_state_chunked_body state, std::span<const std::byte> buffer);

} // namespace detail
} // namespace sl::http::v1::deserialize
