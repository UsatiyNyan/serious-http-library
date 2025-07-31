//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/deserialize_machine.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/meta/monad/maybe.hpp>
#include <sl/meta/monad/result.hpp>
#include <sl/meta/traits/unique.hpp>

#include <cstddef>
#include <span>
#include <variant>
#include <vector>

namespace sl::http::v1::detail::deserialize::request {

struct state_empty;
struct state_method;
struct state_target;
struct state_version;
struct state_fields;
struct state_body;
struct state_chunked_body;
struct state_trailing_fields;
struct state_complete;

using state_variant = std::variant<
    state_empty,
    state_method,
    state_target,
    state_version,
    state_fields,
    state_body,
    state_chunked_body,
    state_trailing_fields,
    state_complete>;

using machine = deserialize::machine<state_variant>;

using process_result = deserialize::process_result<state_variant>;

struct [[nodiscard]] state_empty {
    process_result process(std::span<const std::byte> byte_buffer) &&;
};

struct [[nodiscard]] state_method {
    state_method(state_empty&&, method_type method) : method{ method } {}

    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    method_type method;
};

struct [[nodiscard]] state_target {
    state_target(state_method&& prev_state, target_type target)
        : method{ prev_state.method }, //
          target{ target } {}

    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    method_type method;
    target_type target;
};

struct [[nodiscard]] state_version {
    state_version(state_target&& prev_state, version_type version)
        : method{ prev_state.method }, //
          target{ prev_state.target }, //
          version{ version } {}

    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    method_type method;
    target_type target;
    version_type version;
};

struct [[nodiscard]] state_fields {
    explicit state_fields(state_version&& prev_state)
        : method{ prev_state.method }, //
          target{ prev_state.target }, //
          version{ prev_state.version } {}

    process_result process(std::span<const std::byte> byte_buffer) &&;

private:
    state_variant finalize() &&;


private:
    state_variant process_fields(state_fields state);

public:
    fields_type fields{};
    std::size_t fields_byte_size_total = 0;

    method_type method;
    target_type target;
    version_type version;
};

struct [[nodiscard]] state_body {
    state_body(state_fields&& prev_state, std::size_t content_length)
        : fields{ std::move(prev_state.fields) }, //
          method{ prev_state.method }, //
          target{ prev_state.target }, //
          version{ prev_state.version }, //
          content_length{ content_length } {
        body.reserve(this->content_length);
    }

    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    std::vector<std::byte> body{};

    fields_type fields;
    method_type method;
    target_type target;
    version_type version;

    std::size_t content_length;
};

struct [[nodiscard]] state_chunked_body {
    explicit state_chunked_body(state_fields&& prev_state)
        : fields{ std::move(prev_state.fields) }, //
          method{ prev_state.method }, //
          target{ prev_state.target }, //
          version{ prev_state.version } {}

    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    std::vector<std::byte> chunked_body{};

    fields_type fields;
    method_type method;
    target_type target;
    version_type version;
};

struct [[nodiscard]] state_trailing_fields {
    explicit state_trailing_fields(state_chunked_body&& prev_state)
        : chunked_body{ std::move(prev_state.chunked_body) }, //
          fields{ std::move(prev_state.fields) }, //
          method{ prev_state.method }, //
          target{ prev_state.target }, //
          version{ prev_state.version } {}

    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    std::vector<std::byte> chunked_body;

    fields_type fields;
    method_type method;
    target_type target;
    version_type version;
};

struct [[nodiscard]] state_complete {
    static process_result process_err(status_type status) {
        return process_result::err(state_complete{ meta::err(status) });
    }

    explicit state_complete(state_body&& prev_state)
        : result{ request_message{
              .fields = std::move(prev_state.fields),
              .body = std::move(prev_state.body),
              .target = std::move(prev_state.target),
              .method = prev_state.method,
              .version = prev_state.version,
          } } {}

    explicit state_complete(state_trailing_fields&& prev_state)
        : result{ request_message{
              .fields = std::move(prev_state.fields),
              .body = std::move(prev_state.chunked_body),
              .target = std::move(prev_state.target),
              .method = prev_state.method,
              .version = prev_state.version,
          } } {}

    explicit state_complete(meta::result<request_message, status_type> result) : result{ std::move(result) } {}

public:
    process_result process(std::span<const std::byte> byte_buffer) &&;

public:
    meta::result<request_message, status_type> result;
};

} // namespace sl::http::v1::detail::deserialize::request
