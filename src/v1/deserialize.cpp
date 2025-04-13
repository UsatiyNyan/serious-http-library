//
// Created by usatiynyan.
//
// HTTP-message   = start-line CRLF
//                 *( field-line CRLF )
//                 CRLF
//                 [ message-body ]
//
// start-line     = request-line / status-line
// request-line   = method SP request-target SP HTTP-version
// status-line    = HTTP-version SP status-code SP [ reason-phrase ]

#include "sl/http/v1/deserialize.hpp"
#include "sl/http/v1/detail.hpp"
#include "sl/http/v1/types/request_start.hpp"

#include <sl/meta/enum/from_string.hpp>
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

std::tuple<deserialize_request_state, std::size_t, bool> //
    deserialize_request_process_err(deserialize_request_state state) {
    return std::make_tuple(std::move(state), 0, false);
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process_complete_err(status_type status) {
    return deserialize_request_process_err(deserialize_request_state_complete{ meta::err(status) });
}

// method SP
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_empty state, std::span<const std::byte> byte_buffer) {
    constexpr std::size_t method_max_length = enum_max_str_length<method_type>();
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto method_result = try_find(str_buffer, tokens::SP, method_max_length);
    if (!method_result.has_value()) {
        const auto& method_err = method_result.error();
        if (method_err == find_err::MAX_SIZE_EXCEEDED) {
            return deserialize_request_process_complete_err(status_type::NOT_IMPLEMENTED);
        }
        DEBUG_ASSERT(method_err == find_err::NOT_FOUND);
        return deserialize_request_process_err(std::move(state));
    }

    const auto& [method_str, method_offset] = method_result.value();
    const auto method = meta::enum_from_str<method_type>(method_str);
    if (method == method_type::ENUM_END) {
        return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
    }

    return std::make_tuple(deserialize_request_state_method{ std::move(state), method }, method_offset, true);
}

// request-target SP
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_method state, std::span<const std::byte> byte_buffer) {
    // recommended as per https://www.rfc-editor.org/rfc/rfc9112.html#name-request-line
    constexpr std::size_t target_max_length = 8000;
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto target_result = try_find(str_buffer, tokens::SP, target_max_length);
    if (!target_result.has_value()) {
        const auto& target_err = target_result.error();
        if (target_err == find_err::MAX_SIZE_EXCEEDED) {
            return deserialize_request_process_complete_err(status_type::URI_TOO_LONG);
        }
        DEBUG_ASSERT(target_err == find_err::NOT_FOUND);
        return deserialize_request_process_err(std::move(state));
    }
    const auto& [target_str, target_offset] = target_result.value();

    auto maybe_target = deserialize_request_target(target_str);
    if (!maybe_target.has_value()) {
        return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
    }

    return std::make_tuple(
        deserialize_request_state_target{ std::move(state), std::move(maybe_target).value() }, target_offset, true
    );
}

// HTTP-version CRLF
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_target state, std::span<const std::byte> byte_buffer) {
    constexpr std::size_t version_max_length = enum_max_str_length<version_type>();
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto version_result = try_find(str_buffer, tokens::CRLF, version_max_length);
    if (!version_result.has_value()) {
        const auto& version_err = version_result.error();
        if (version_err == find_err::MAX_SIZE_EXCEEDED) {
            return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(version_err == find_err::NOT_FOUND);
        return deserialize_request_process_err(std::move(state));
    }

    const auto& [version_str, version_offset] = version_result.value();
    const auto version = meta::enum_from_str<version_type>(version_str);
    if (version == version_type::ENUM_END) {
        return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
    }

    return std::make_tuple(deserialize_request_state_version{ std::move(state), version }, version_offset, true);
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process(
    deserialize_request_state_version state,
    std::span<const std::byte> byte_buffer [[maybe_unused]]
) {
    return std::make_tuple(deserialize_request_state_fields{ std::move(state) }, 0, true);
}

// *( field-line CRLF ) CRLF
// field-line   = field-name ":" OWS field-value OWS
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_fields state, std::span<const std::byte> byte_buffer) {
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    // std::string_view fields_remainder = start_line_remainder;
    //
    // while (true) {
    //     const auto field_line_result = try_find(fields_remainder, tokens::CRLF);
    //     if (!field_line_result.has_value()) {
    //         return meta::null;
    //     }
    //     const auto& [field_line, field_line_remainder] = field_line_result.value();
    //
    //     fields_remainder = field_line_remainder;
    //     if (field_line.empty()) { // detected last CRLF
    //         break;
    //     }
    //
    //     const auto field_kv_result = deserialize_field(field_line);
    //     if (!field_kv_result.has_value()) {
    //         return meta::null;
    //     }
    //     const auto& [field_key, field_value] = field_kv_result.value();
    //
    //     const auto [field_kv_it, field_kv_is_emplaced] = fields.try_emplace(field_key, field_value);
    //     if (std::ignore = field_kv_it; !field_kv_is_emplaced) {
    //         return meta::null;
    //     }
    // }
    //
    // return std::make_pair(fields, fields_remainder);

    state.buffer.insert(state.buffer.end(), byte_buffer.begin(), byte_buffer.end());
    state.fields;
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_body state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_chunked_body state, std::span<const std::byte> byte_buffer) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process(
    deserialize_request_state_trailing_fields state,
    std::span<const std::byte> byte_buffer
) {
    // TODO
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process(
    deserialize_request_state_complete state,
    std::span<const std::byte> byte_buffer [[maybe_unused]]
) {
    return std::make_tuple(std::move(state), 0, false);
}

std::tuple<deserialize_request_state, deserialize_request_remainder> deserialize_request_machine(
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
                    auto [state, offset, can_process] = deserialize_request_process(std::move(x), byte_buffer);
                    return std::make_tuple(std::move(state), byte_buffer.subspan(offset), can_process);
                } };
            if (!can_process) {
                break;
            }
        }
    }
#endif

    std::ignore = remainder.merge(byte_buffer);

    while (true) {
        std::size_t new_offset = 0;
        bool can_process = true;
        std::tie(state, new_offset, can_process) =
            std::move(state) | meta::pmatch{ [remainder_view = remainder.view()](auto&& x) {
                return deserialize_request_process(std::move(x), remainder_view);
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
        detail::deserialize_request_machine(std::move(state_), std::move(remainder_), byte_buffer);
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
