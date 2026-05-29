//
// Created by usatiynyan.
//

#pragma once

#include "sl/http/v1/detail/machine.hpp"
#include "sl/http/v1/types.hpp"

#include <sl/meta/func/function.hpp>

namespace sl::http::v1 {

struct write_config {
    std::size_t buffer_size = 1024;
    // bool is_chunked = false; TODO
};

namespace detail {

struct write_state_start_line {};
struct write_state_fields {
    fields_type::const_iterator it;
};
struct write_state_body {
    std::size_t offset = 0;
};
// struct write_state_chunked_body {}; TODO
// struct write_state_trailing_fields {}; TODO
struct write_state_complete {};

using write_state = std::variant<
    write_state_start_line,
    write_state_fields,
    write_state_body,
    // write_state_chunked_body, TODO
    // write_state_trailing_fields, TODO
    write_state_complete>;

struct write_machine {
    constexpr write_machine(const message_type& message, write_config config)
        : remainder_{ config.buffer_size }, state_{ write_state_start_line{} }, config_{ std::move(config) },
          message_{ message } {}

    std::span<const std::byte> resume(std::size_t written) &;

public: // transparent
    static write_state resume_impl(
        const message_type& message,
        remainder_buffer<>& remainder,
        const write_config& config,
        const write_state_start_line& state
    );
    static write_state resume_impl(
        const message_type& message,
        remainder_buffer<>& remainder,
        const write_config& config,
        const write_state_fields& state
    );
    static write_state resume_impl(
        const message_type& message,
        remainder_buffer<>& remainder,
        const write_config& config,
        const write_state_body& state
    );

private:
    remainder_buffer<> remainder_;
    write_state state_;
    write_config config_;
    const message_type& message_;
};

std::error_code verify(const message_type& message);

} // namespace detail
} // namespace sl::http::v1
