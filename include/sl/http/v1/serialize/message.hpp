//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/machine.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/meta/func/function.hpp>

namespace sl::http::v1 {

struct serialize_config {
    std::size_t buffer_size = 1024;
    // bool is_chunked = false; TODO
};

meta::unique_function<std::span<const std::byte>(std::size_t written)>
    make_serialize(const message_type& message, serialize_config config);

namespace detail {

struct serialize_state_start_line {};
struct serialize_state_fields {
    fields_type::const_iterator it;
};
struct serialize_state_body {
    std::size_t offset = 0;
};
// struct serialize_state_chunked_body {}; TODO
// struct serialize_state_trailing_fields {}; TODO
struct serialize_state_complete {};

using serialize_state = std::variant<
    serialize_state_start_line,
    serialize_state_fields,
    serialize_state_body,
    // serialize_state_chunked_body, TODO
    // serialize_state_trailing_fields, TODO
    serialize_state_complete>;

struct serialize_machine {
    constexpr serialize_machine(const message_type& message, serialize_config config)
        : remainder_{ config.buffer_size }, state_{ serialize_state_start_line{} }, config_{ std::move(config) },
          message_{ message } {}

    std::span<const std::byte> resume(std::size_t written) &;

public: // transparent
    static serialize_state resume_impl(
        const message_type& message,
        remainder_buffer<>& remainder,
        const serialize_config& config,
        const serialize_state_start_line& state
    );
    static serialize_state resume_impl(
        const message_type& message,
        remainder_buffer<>& remainder,
        const serialize_config& config,
        const serialize_state_fields& state
    );
    static serialize_state resume_impl(
        const message_type& message,
        remainder_buffer<>& remainder,
        const serialize_config& config,
        const serialize_state_body& state
    );

private:
    remainder_buffer<> remainder_;
    serialize_state state_;
    serialize_config config_;
    const message_type& message_;
};

std::error_code verify(const message_type& message);

} // namespace detail
} // namespace sl::http::v1
