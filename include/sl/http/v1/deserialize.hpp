//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/types.hpp"
#include "sl/http/v1/types/status.hpp"

#include <sl/meta/lifetime/unique.hpp>
#include <sl/meta/monad/maybe.hpp>
#include <sl/meta/monad/result.hpp>

#include <cstddef>
#include <span>
#include <variant>

#ifndef SL_HTTP_V1_DESERIALIZE_REQUEST_OPTIMIZATION
#define SL_HTTP_V1_DESERIALIZE_REQUEST_OPTIMIZATION 1
#endif

namespace sl::http::v1 {
namespace detail {

struct deserialize_request_remainder {
    std::span<const std::byte> view() const;
    [[nodiscard]] std::size_t merge(std::span<const std::byte> byte_buffer);
    void add_offset(std::size_t offset) { offset_ += offset; }

private:
    std::vector<std::byte> buffer_{};
    std::size_t offset_ = 0;
};

struct deserialize_request_state_empty : meta::unique {
    std::vector<std::byte> buffer;
};

struct deserialize_request_state_method : deserialize_request_state_empty {
    deserialize_request_state_method(deserialize_request_state_empty&& prev_state, method_type method) noexcept
        : deserialize_request_state_empty{ std::move(prev_state) }, method{ method } {}

public:
    method_type method;
};

struct deserialize_request_state_target : deserialize_request_state_method {
    deserialize_request_state_target(deserialize_request_state_method&& prev_state, target_type target) noexcept
        : deserialize_request_state_method{ std::move(prev_state) }, target{ target } {}

public:
    target_type target;
};

struct deserialize_request_state_version : deserialize_request_state_target {
    deserialize_request_state_version(deserialize_request_state_target&& prev_state, version_type version) noexcept
        : deserialize_request_state_target{ std::move(prev_state) }, version{ version } {}

public:
    version_type version;
};

struct deserialize_request_state_fields : deserialize_request_state_version {
    explicit deserialize_request_state_fields(deserialize_request_state_version&& prev_state) noexcept
        : deserialize_request_state_version{ std::move(prev_state) } {}

public:
    fields_type fields;
};

struct deserialize_request_state_body : deserialize_request_state_fields {
    explicit deserialize_request_state_body(deserialize_request_state_fields&& prev_state) noexcept
        : deserialize_request_state_fields{ std::move(prev_state) } {}

public:
    std::span<const std::byte> body;
};

struct deserialize_request_state_chunked_body : deserialize_request_state_fields {
    explicit deserialize_request_state_chunked_body(deserialize_request_state_fields&& prev_state) noexcept
        : deserialize_request_state_fields{ std::move(prev_state) } {}

public:
    std::span<const std::byte> chunked_body;
};

struct deserialize_request_state_trailing_fields : deserialize_request_state_chunked_body {
    explicit deserialize_request_state_trailing_fields(deserialize_request_state_chunked_body&& prev_state) noexcept
        : deserialize_request_state_chunked_body{ std::move(prev_state) } {}

    // populate fields;
};

struct deserialize_request_state_complete : deserialize_request_state_empty {
    explicit deserialize_request_state_complete(deserialize_request_state_body&& prev_state) noexcept
        : result{ request_message{
              .fields = std::move(prev_state.fields),
              .body = std::move(prev_state.body),
              .target = std::move(prev_state.target),
              .method = prev_state.method,
              .version = prev_state.version,
          } } {}

    explicit deserialize_request_state_complete(deserialize_request_state_trailing_fields&& prev_state) noexcept
        : result{ request_message{
              .fields = std::move(prev_state.fields),
              .body = std::move(prev_state.chunked_body),
              .target = std::move(prev_state.target),
              .method = prev_state.method,
              .version = prev_state.version,
          } } {}

    explicit deserialize_request_state_complete(meta::result<request_message, status_type> result)
        : result{ std::move(result) } {}

public:
    meta::result<request_message, status_type> result;
};

using deserialize_request_state = std::variant<
    deserialize_request_state_empty,
    deserialize_request_state_method,
    deserialize_request_state_target,
    deserialize_request_state_version,
    deserialize_request_state_fields,
    deserialize_request_state_body,
    deserialize_request_state_chunked_body,
    deserialize_request_state_trailing_fields,
    deserialize_request_state_complete>;

struct deserialize_request_process_result {
    deserialize_request_state state;
    std::size_t offset;
    bool can_process;
};

std::tuple<deserialize_request_state, std::size_t, bool> //
    deserialize_request_process_err(deserialize_request_state state);
std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process_complete_err(status_type status);

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_empty state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_method state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_target state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_version state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_fields state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_body state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_chunked_body state, std::span<const std::byte> byte_buffer);
std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process(
    deserialize_request_state_trailing_fields state,
    std::span<const std::byte> byte_buffer
);
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_complete state, std::span<const std::byte> byte_buffer);

std::tuple<deserialize_request_state, deserialize_request_remainder> deserialize_request_machine(
    deserialize_request_state state,
    deserialize_request_remainder remainder,
    std::span<const std::byte> byte_buffer
);

} // namespace detail

struct request_deserializer {
    [[nodiscard]] bool keep_alive() const& { return keep_alive_; }
    void close() & { keep_alive_ = false; }

    void read(std::span<const std::byte> byte_buffer) &;
    [[nodiscard]] bool has_next() const&;
    [[nodiscard]] meta::result<request_message, status_type> next() &;

private:
    detail::deserialize_request_state state_ = detail::deserialize_request_state_empty{};
    detail::deserialize_request_remainder remainder_;
    bool keep_alive_ = true;
};

} // namespace sl::http::v1
