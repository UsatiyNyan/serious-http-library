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

using message_result = meta::result<message_type, status_type>;

struct config_type {
    std::size_t max_body_size = 1 * 1024 * 1024; // 1 MiB default
    std::size_t max_field_size = 80 * 1024; // 80 KiB default
};

struct message_chunk {
    const message_type& message;
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};

exec::async_gen<message_chunk, io_result<message_result>>
    request(exec::async_gen<std::span<const std::byte>, std::error_code> input, const config_type& config = {});

namespace detail {

struct state_start_line_request_method {};
struct state_start_line_request_target {};
struct state_start_line_request_version {};

using state_start_line_request = std::variant< //
    state_start_line_request_method,
    state_start_line_request_target,
    state_start_line_request_version>;

struct state_fields {
    std::size_t consumed_bytes = 0;
};
struct state_body {
    std::size_t content_length = 0;
};

struct state_chunked_body_empty {};
struct state_chunked_body_line {
    std::string chunk_ext;
    std::uint32_t chunk_size = 0;
};
struct state_chunked_body_complete {
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};
using state_chunked_body = std::variant< //
    state_chunked_body_empty,
    state_chunked_body_line,
    state_chunked_body_complete>;

struct state_trailing_fields {
    std::size_t consumed_bytes = 0;
};
struct state_complete {};

using state = std::variant<
    state_start_line_request,
    state_fields,
    state_body,
    state_chunked_body,
    state_trailing_fields,
    state_complete>;

inline parse_result<meta::result<state, status_type>> make_parse_stop(status_type st) {
    return parse_result<meta::result<state, status_type>>::stop(meta::err(st));
}
inline parse_result<meta::result<state, status_type>> make_parse_stop(state s) {
    return parse_result<meta::result<state, status_type>>::stop(s);
}
inline parse_result<meta::result<state, status_type>> make_parse_more(state s, std::size_t offset) {
    return parse_result<meta::result<state, status_type>>::more(s, offset);
}

inline parse_result<meta::result<state_chunked_body, status_type>> make_parse_chunk_stop(state_chunked_body s) {
    return parse_result<meta::result<state_chunked_body, status_type>>::stop(s);
}
inline parse_result<meta::result<state_chunked_body, status_type>> make_parse_chunk_stop(status_type status) {
    return parse_result<meta::result<state_chunked_body, status_type>>::stop(meta::err(status));
}
inline parse_result<meta::result<state_chunked_body, status_type>>
    make_parse_chunk_more(state_chunked_body s, std::size_t offset) {
    return parse_result<meta::result<state_chunked_body, status_type>>::more(s, offset);
}

parse_result<meta::result<state, status_type>>
    parse_request(message_type& output, state s, std::span<const std::byte> byte_buffer, const config_type& config);

parse_result<meta::result<state, status_type>>
    parse_part(message_type& output, state_start_line_request s, std::string_view buffer);

parse_result<meta::result<state, status_type>> parse_part(
    message_type& output,
    std::variant<state_fields, state_trailing_fields> s,
    std::string_view buffer,
    const config_type& config
);
meta::result<state, status_type>
    parse_fields_finalize(message_type& output, std::size_t fields_consumed_bytes, const config_type& config);

parse_result<meta::result<state, status_type>>
    parse_part(message_type& output, state_body s, std::span<const std::byte> buffer);

parse_result<meta::result<state_chunked_body, status_type>>
    parse_chunk(message_type& output, state_chunked_body s, std::span<const std::byte> buffer);

} // namespace detail
} // namespace sl::http::v1::deserialize
