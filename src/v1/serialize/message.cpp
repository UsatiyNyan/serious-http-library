//
// Created by usatiynyan.
//

#include "sl/http/v1/serialize/message.hpp"
#include "sl/http/v1/detail/strings.hpp"
#include "sl/http/v1/serialize/target.hpp"

#include <sl/meta/match/overloaded.hpp>
#include <variant>

namespace sl::http::v1 {

meta::unique_function<std::span<const std::byte>(std::size_t written)>
    make_serialize(const message_type& message, serialize_config config) {
    return [machine = detail::serialize_machine{ message, config }](std::size_t written) mutable {
        return machine.resume(written);
    };
}

namespace detail {

std::span<const std::byte> serialize_machine::resume(std::size_t written) & {
    if (!remainder_.view().empty()) {
        remainder_.add_offset(written);
        return remainder_.view();
    }

    if (std::holds_alternative<serialize_state_complete>(state_)) {
        return {};
    }

    if (auto* state = std::get_if<serialize_state_body>(&state_)) {
        state->offset += written;
        return std::span{ message_.body }.subspan(state->offset);
    }

    while (remainder_.view().size_bytes() < config_.buffer_size //
           && !std::holds_alternative<serialize_state_complete>(state_)) {
        state_ = std::visit(
            meta::overloaded{
                [](const serialize_state_complete& state) { return serialize_state{ state }; },
                [this](const auto& state) { return resume_impl(message_, remainder_, config_, state); },
            },
            state_
        );
    }

    return remainder_.view();
}

serialize_state serialize_machine::resume_impl(
    const message_type& message,
    remainder_buffer<>& remainder,
    const serialize_config& config,
    const serialize_state_start_line& state
) {
    const auto write = [&remainder](std::string_view str) { std::ignore = remainder.merge(buffer_str_to_byte(str)); };

    std::visit(
        meta::overloaded{
            [&](const request_line_type& req) {
                const auto target = serialize(req.target);
                write(enum_to_str(req.method));
                write(tokens::SP);
                write(target);
                write(tokens::SP);
                write(enum_to_str(req.version));
            },
            [&](const response_line_type& res) {
                write(enum_to_str(res.version));
                write(tokens::SP);
                write(enum_to_str(res.status));
                write(tokens::SP);
                if (!res.reason.empty()) {
                    write(res.reason);
                }
            },
        },
        message.start_line
    );
    write(tokens::CRLF);

    return serialize_state_fields{ .it = message.fields.begin() };
}

serialize_state serialize_machine::resume_impl(
    const message_type& message,
    remainder_buffer<>& remainder,
    const serialize_config& config,
    const serialize_state_fields& state
) {
    const auto write = [&remainder](std::string_view str) { std::ignore = remainder.merge(buffer_str_to_byte(str)); };

    if (state.it == message.fields.end()) {
        write(tokens::CRLF);
        return serialize_state_body{ .offset = 0 };
    }

    write(state.it.key());
    write(tokens::COLON);
    write(state.it.value());
    write(tokens::CRLF);

    return serialize_state_fields{ .it = std::next(state.it) };
}

serialize_state serialize_machine::resume_impl(
    const message_type& message,
    remainder_buffer<>& remainder,
    const serialize_config& config,
    const serialize_state_body& state
) {
    if (state.offset == message.body.size()) {
        return serialize_state_complete{};
    }

    DEBUG_ASSERT(remainder.view().size_bytes() < config.buffer_size);
    const std::size_t offset = std::min(config.buffer_size - remainder.view().size_bytes(), message.body.size());
    std::ignore = remainder.merge(std::span{ message.body }.subspan(0, offset));
    return serialize_state_body{ .offset = offset };
}

std::error_code verify(const message_type& message) {
    if (const auto* response_line = std::get_if<response_line_type>(&message.start_line)) {
        const auto& reason = response_line->reason;
        // RFC 7230: reason-phrase = *( HTAB / SP / VCHAR / obs-text )
        // Must not contain CR or LF
        const auto it = std::find_if(reason.begin(), reason.end(), [](char c) { return c == '\r' || c == '\n'; });
        if (it != reason.end()) {
            return std::make_error_code(std::errc::invalid_argument);
        }
    }
    return {};
}

} // namespace detail
} // namespace sl::http::v1
