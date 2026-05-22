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

struct parse_config {
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
    make_parse_request(parse_config config);

meta::unique_function<meta::maybe<status_type>(std::span<const std::byte> input)>
    make_parse_response(parse_config config);

namespace detail {

struct parse_state_start_line_request_method {};
struct parse_state_start_line_request_target {};
struct parse_state_start_line_request_version {};
using parse_state_start_line_request = std::variant< //
    parse_state_start_line_request_method,
    parse_state_start_line_request_target,
    parse_state_start_line_request_version>;

struct parse_state_start_line_response_version {};
struct parse_state_start_line_response_status {};
struct parse_state_start_line_response_reason {};
using parse_state_start_line_response = std::variant< //
    parse_state_start_line_response_version,
    parse_state_start_line_response_status,
    parse_state_start_line_response_reason>;

using parse_state_start_line = std::variant<parse_state_start_line_request, parse_state_start_line_response>;

struct parse_state_fields {
    std::size_t consumed_bytes = 0;
};
struct parse_state_body {
    std::size_t content_length = 0;
};

struct parse_state_chunked_body_empty {};
struct parse_state_chunked_body_line {
    std::string chunk_ext;
    std::uint32_t chunk_size = 0;
};
struct parse_state_chunked_body_complete {
    std::string chunk_ext;
    std::span<const std::byte> chunk;
};
using parse_state_chunked_body = std::variant< //
    parse_state_chunked_body_empty,
    parse_state_chunked_body_line,
    parse_state_chunked_body_complete>;

struct parse_state_trailing_fields {
    std::size_t consumed_bytes = 0;
};
struct parse_state_complete {};

using parse_state = std::variant< //
    parse_state_start_line,
    parse_state_fields,
    parse_state_body,
    parse_state_chunked_body,
    parse_state_trailing_fields,
    parse_state_complete>;

struct parse_ok {
    // continue is offset > 0 or offset == continue_token
    static constexpr std::size_t continue_token = std::numeric_limits<std::size_t>::max();
    static parse_ok stop(parse_state state) {
        return parse_ok{
            .state = std::move(state),
            .offset = 0,
        };
    }

public:
    parse_state state;
    std::size_t offset;
};

struct parse_machine {
    constexpr explicit parse_machine(parse_config config, bool is_request) : config_{ std::move(config) } {
        DEBUG_ASSERT(!!config_.message_cb);
        if (is_request) {
            output_.start_line = request_line_type{};
        } else {
            output_.start_line = response_line_type{};
        }
    }

    meta::maybe<status_type> parse(std::span<const std::byte> input) &;

private: // only dispatch and mutation
    meta::result<std::size_t, status_type> parse_impl(std::span<const std::byte> input) &;

public: // transparent
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line state,
        const parse_config& config,
        std::span<const std::byte> input
    );

    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_request state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_request_method state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_request_target state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_request_version state,
        const parse_config& config,
        std::span<const std::byte> input
    );

    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_response state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_response_version state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_response_status state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_start_line_response_reason state,
        const parse_config& config,
        std::span<const std::byte> input
    );

    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_fields state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_trailing_fields state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        std::variant<parse_state_fields, parse_state_trailing_fields> state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_state, status_type>
        parse_state_fields_finalize(message_type& output, const parse_config& config);

    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_body state,
        const parse_config& config,
        std::span<const std::byte> input
    );

    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_chunked_body state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_chunked_body_empty state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_chunked_body_line state,
        const parse_config& config,
        std::span<const std::byte> input
    );
    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_chunked_body_complete state,
        const parse_config& config,
        std::span<const std::byte> input
    );

    static meta::result<parse_ok, status_type> parse_impl(
        message_type& output,
        parse_state_complete state,
        const parse_config& config,
        std::span<const std::byte> input
    );

private:
    message_type output_{};
    remainder_buffer<> remainder_{}; // TODO: extract outside
    parse_state state_;
    parse_config config_;
};

} // namespace detail
} // namespace sl::http::v1
