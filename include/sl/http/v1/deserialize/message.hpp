//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/machine.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/meta/func/function.hpp>
#include <sl/meta/monad/result.hpp>
#include <sl/meta/type/unit.hpp>

#include <cstddef>
#include <limits>
#include <span>

namespace sl::http::v1 {

struct message_chunk {
    const message_type& message;
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};

struct deserialize_config {
    meta::unique_function<void(message_chunk)> chunk_cb;
    meta::unique_function<void(message_type)> message_cb;

    std::size_t max_body_size = 1 * 1024 * 1024; // 1 MiB default
    std::size_t max_field_size = 80 * 1024; // 80 KiB default
    std::size_t max_reason_size = 8000; // recommended as per RFC 9112
    std::size_t max_target_size = 8000; // recommended as per RFC 9112
    std::size_t max_chunk_size_size = 8;
    std::size_t max_chunk_line_size = max_chunk_size_size + 8 * 1024; // 8B + 8KiB
};

meta::unique_function<meta::maybe<status_type>(std::span<const std::byte> input)>
    make_deserialize_request(deserialize_config config);

meta::unique_function<meta::maybe<status_type>(std::span<const std::byte> input)>
    make_deserialize_response(deserialize_config config);

namespace detail {

struct deserialize_state_start_line_request_method {};
struct deserialize_state_start_line_request_target {};
struct deserialize_state_start_line_request_version {};
using deserialize_state_start_line_request = std::variant< //
    deserialize_state_start_line_request_method,
    deserialize_state_start_line_request_target,
    deserialize_state_start_line_request_version>;

struct deserialize_state_start_line_response_version {};
struct deserialize_state_start_line_response_status {};
struct deserialize_state_start_line_response_reason {};
using deserialize_state_start_line_response = std::variant< //
    deserialize_state_start_line_response_version,
    deserialize_state_start_line_response_status,
    deserialize_state_start_line_response_reason>;

using deserialize_state_start_line =
    std::variant<deserialize_state_start_line_request, deserialize_state_start_line_response>;

struct deserialize_state_fields {
    std::size_t consumed_bytes = 0;
};
struct deserialize_state_body {
    std::size_t content_length = 0;
};

struct deserialize_state_chunked_body_empty {};
struct deserialize_state_chunked_body_line {
    std::string chunk_ext;
    std::uint32_t chunk_size = 0;
};
struct deserialize_state_chunked_body_complete {
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};
using deserialize_state_chunked_body = std::variant< //
    deserialize_state_chunked_body_empty,
    deserialize_state_chunked_body_line,
    deserialize_state_chunked_body_complete>;

struct deserialize_state_trailing_fields {
    std::size_t consumed_bytes = 0;
};
struct deserialize_state_complete {};

using deserialize_state = std::variant< //
    deserialize_state_start_line,
    deserialize_state_fields,
    deserialize_state_body,
    deserialize_state_chunked_body,
    deserialize_state_trailing_fields,
    deserialize_state_complete>;

struct deserialize_ok {
    // continue is offset > 0 or offset == continue_token
    static constexpr std::size_t continue_token = std::numeric_limits<std::size_t>::max();
    static deserialize_ok stop(deserialize_state state) {
        return deserialize_ok{
            .state = std::move(state),
            .offset = 0,
        };
    }

public:
    deserialize_state state;
    std::size_t offset;
};

struct deserialize_machine {
    constexpr deserialize_machine(deserialize_config config, bool is_request) : config_{ std::move(config) } {
        DEBUG_ASSERT(!!config_.message_cb);
        if (is_request) {
            output_.start_line = request_line_type{};
        } else {
            output_.start_line = response_line_type{};
        }
    }

    meta::maybe<status_type> deserialize(std::span<const std::byte> input) &;

private: // only dispatch and mutation
    meta::result<std::size_t, status_type> deserialize_impl(std::span<const std::byte> input) &;

public: // transparent
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );

    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_request state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_request_method state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_request_target state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_request_version state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );

    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_response state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_response_version state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_response_status state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_start_line_response_reason state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );

    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_fields state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_trailing_fields state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        std::variant<deserialize_state_fields, deserialize_state_trailing_fields> state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_state, status_type>
        deserialize_state_fields_finalize(message_type& output, const deserialize_config& config);

    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_body state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );

    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_chunked_body state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_chunked_body_empty state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_chunked_body_line state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );
    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_chunked_body_complete state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );

    static meta::result<deserialize_ok, status_type> deserialize_impl(
        message_type& output,
        deserialize_state_complete state,
        const deserialize_config& config,
        std::span<const std::byte> input
    );

private:
    message_type output_{};
    remainder_buffer<> remainder_{}; // TODO: extract outside
    deserialize_state state_;
    deserialize_config config_;
};

} // namespace detail
} // namespace sl::http::v1
