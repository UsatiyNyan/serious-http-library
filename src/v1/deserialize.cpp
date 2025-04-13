//
// Created by usatiynyan.
//
// HTTP-message   = start-line CRLF
//                 *( field-line CRLF )
//                 CRLF
//                 [ message-body ]
//

#include "sl/http/v1/deserialize.hpp"
#include "sl/http/v1/detail.hpp"

#include <sl/meta/match/pmatch.hpp>

#include <libassert/assert.hpp>

namespace sl::http::v1 {
namespace detail {

std::span<const std::byte> deserialize_request_remainder::view() const {
    if (ASSUME_VAL(buffer_.size() > offset_)) {
        return std::span{ buffer_ }.subspan(offset_);
    }
    return {};
}

std::size_t deserialize_request_remainder::merge(std::span<const std::byte> byte_buffer) {
    const std::size_t offset = std::exchange(offset_, 0);
    DEBUG_ASSERT(buffer_.size() >= offset);
    if (buffer_.size() == offset) {
        buffer_.assign(byte_buffer.begin(), byte_buffer.end());
    } else {
        buffer_.erase(buffer_.begin(), std::next(buffer_.begin(), offset));
        buffer_.insert(buffer_.end(), byte_buffer.begin(), byte_buffer.end());
    }
    return offset;
}

void deserialize_request_state_empty::shift_visited_bytes(std::size_t prev_offset) {
    if (!maybe_visited_bytes.has_value()) {
        return;
    }
    std::size_t& visited_bytes = maybe_visited_bytes.value();
    if (ASSUME_VAL(visited_bytes >= prev_offset)) {
        visited_bytes -= prev_offset;
    } else {
        maybe_visited_bytes.reset();
    }
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_state_process(deserialize_request_state_empty state, std::span<const std::byte> byte_buffer) {
    // const auto start_line_result = try_find_with_remainder(str_buffer, detail::tokens::CRLF);
    // if (!start_line_result.has_value()) {
    //     return meta::null;
    // }
    // const auto& [start_line, start_line_remainder] = start_line_result.value();
    //
    // const auto start_result = deserialize_request_start(start_line);
    // if (!start_result.has_value()) {
    //     return meta::null;
    // }
    //
    // auto fields_result = deserialize_fields(start_line_remainder);
    // if (!fields_result.has_value()) {
    //     return meta::null;
    // }
    // auto& [fields, fields_remainder] = fields_result.value();
    //
    // const auto body = buffer_str_to_byte(fields_remainder);
    //
    // return request_message{
    //     .start = start_result.value(),
    //     .fields = std::move(fields),
    //     .body = body,
    // };
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_state_process(deserialize_request_state_method state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_state_process(deserialize_request_state_target state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_state_process(deserialize_request_state_version state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_state_process(deserialize_request_state_fields state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_state_process(deserialize_request_state_body state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_state_process(
    deserialize_request_state_chunked_body state,
    std::span<const std::byte> byte_buffer
) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_state_process(
    deserialize_request_state_trailing_fields state,
    std::span<const std::byte> byte_buffer
) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_state_process(
    deserialize_request_state_complete state,
    std::span<const std::byte> byte_buffer [[maybe_unused]]
) {
    return std::make_tuple(std::move(state), 0, false);
}

std::tuple<deserialize_request_state, deserialize_request_remainder> deserialize_request_state_machine(
    deserialize_request_state state,
    deserialize_request_remainder remainder,
    std::span<const std::byte> byte_buffer
) {
#if SL_HTTP_V1_DESERIALIZE_REQUEST_OPTIMIZATION // optimization, needs testing
    if (remainder.view().empty()) { // less allocations and copying
        while (!byte_buffer.empty()) {
            bool can_process = true;
            std::tie(state, byte_buffer, can_process) = //
                std::move(state) | meta::pmatch{ [byte_buffer](auto&& x) {
                    auto [state, offset, can_process] = deserialize_request_state_process(std::move(x), byte_buffer);
                    return std::make_tuple(std::move(state), byte_buffer.subspan(offset), can_process);
                } };
            if (!can_process) {
                break;
            }
        }
    }
#endif

    const std::size_t prev_offset = remainder.merge(byte_buffer);
    state | meta::pmatch{ [prev_offset](deserialize_request_state_empty& x) { x.shift_visited_bytes(prev_offset); } };

    while (true) {
        std::size_t new_offset = 0;
        bool can_process = true;
        std::tie(state, new_offset, can_process) =
            std::move(state) | meta::pmatch{ [remainder_view = remainder.view()](auto&& x) {
                return deserialize_request_state_process(std::move(x), remainder_view);
            } };
        remainder.add_offset(new_offset);
        if (!can_process) {
            break;
        }
    }

    return std::make_tuple(std::move(state), std::move(remainder));
}

} // namespace detail

void request_deserializer::read(std::span<const std::byte> byte_buffer) & {
    std::tie(state_, remainder_) =
        detail::deserialize_request_state_machine(std::move(state_), std::move(remainder_), byte_buffer);
}

bool request_deserializer::has_next() const& {
    return std::holds_alternative<detail::deserialize_request_state_complete>(state_);
}

meta::result<request_message, status_type> request_deserializer::next() & {
    auto* state_complete_ptr = std::get_if<detail::deserialize_request_state_complete>(&state_);
    const bool state_complete_is_present = nullptr != state_complete_ptr;
    if (!ASSUME_VAL(state_complete_is_present)) {
        return meta::err(status_type::INTERNAL_SERVER_ERROR);
    }
    auto result = std::move(state_complete_ptr->result);
    state_ = detail::deserialize_request_state_empty{};
    read(std::span<const std::byte>{});
    return result;
}

} // namespace sl::http::v1
