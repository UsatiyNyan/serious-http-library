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

#include <charconv>

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

// TODO: visited_bytes
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

    const auto method_result = try_find_limited(str_buffer, tokens::SP, method_max_length);
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

    const auto target_result = try_find_limited(str_buffer, tokens::SP, target_max_length);
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

    const auto version_result = try_find_limited(str_buffer, tokens::CRLF, version_max_length);
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

deserialize_request_state deserialize_request_process_fields(deserialize_request_state_fields state) {
    const auto find_field = [&](auto k) -> meta::maybe<std::string_view> {
        auto it = state.fields.find(k); // can avoid string creation
        if (it == state.fields.end()) {
            return meta::null;
        }
        return std::string_view{ it.value() };
    };

    const auto maybe_transfer_encodings = find_field("Transfer-Encoding");
    const auto maybe_content_length_str = find_field("Content-Length");

    const bool is_chunked = maybe_transfer_encodings
                                .map([](std::string_view transfer_encodings) {
                                    while (const auto maybe_next = try_find_split(transfer_encodings, ", ")) {
                                        const auto [transfer_encoding, remainder] = maybe_next.value();
                                        if (transfer_encoding == "chunked") {
                                            return true;
                                        }
                                        transfer_encodings = remainder;
                                    }
                                    return false;
                                })
                                .value_or(false);

    if (is_chunked) {
        if (maybe_content_length_str.has_value()) {
            // TODO: or is it?
            return deserialize_request_state_complete{ meta::err(status_type::BAD_REQUEST) };
        }

        return deserialize_request_state_chunked_body{ std::move(state) };
    }

    const auto maybe_content_length =
        maybe_content_length_str.and_then([](std::string_view content_length_str) -> meta::maybe<std::size_t> {
            std::size_t content_length = 0;
            const auto conv_result =
                std::from_chars(content_length_str.begin(), content_length_str.end(), content_length);
            if (conv_result.ec != std::error_code{} || conv_result.ptr != content_length_str.end()) {
                return meta::null;
            }
            return content_length;
        });

    if (!maybe_content_length.has_value()) {
        return deserialize_request_state_complete{ meta::err(status_type::BAD_REQUEST) };
    }
    const std::size_t content_length = maybe_content_length.value();

    return deserialize_request_state_body{ std::move(state), content_length };
}

// *( field-line CRLF ) CRLF
// field-line   = field-name ":" OWS field-value OWS
std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_fields state, std::span<const std::byte> byte_buffer) {
    constexpr std::size_t max_fields_size = 80 * 1024; // 80KiB
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    const auto field_line_result =
        try_find_limited(str_buffer, tokens::CRLF, max_fields_size - state.fields_byte_size_total);
    if (!field_line_result.has_value()) {
        const auto& field_line_err = field_line_result.error();
        if (field_line_err == find_err::MAX_SIZE_EXCEEDED) {
            return deserialize_request_process_complete_err(status_type::CONTENT_TOO_LARGE);
        }
        DEBUG_ASSERT(field_line_err == find_err::NOT_FOUND);
        return deserialize_request_process_err(std::move(state));
    }
    const auto& [field_line, field_line_offset] = field_line_result.value();

    if (field_line.empty()) { // detected last CRLF
        return std::make_tuple(deserialize_request_process_fields(std::move(state)), field_line_offset, true);
    }

    // not limiting by max_size since field_line_result is already limited
    const auto field_kv_result = try_find_unlimited(field_line, ":");
    if (!field_kv_result.has_value()) {
        return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
    }
    const auto& [field_key, field_offset] = field_kv_result.value();
    constexpr auto strip = [](std::string_view x) {
        x = strip_prefix(x, tokens::SP);
        x = strip_prefix(x, tokens::HTAB);
        x = strip_suffix(x, tokens::SP);
        x = strip_suffix(x, tokens::HTAB);
        return x;
    };
    const auto field_value = strip(field_line.substr(field_offset));

    const auto [field_kv_it, field_kv_is_emplaced] = state.fields.try_emplace(std::string{ field_key }, field_value);
    std::ignore = field_kv_it;
    if (!field_kv_is_emplaced) {
        return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
    }

    state.fields_byte_size_total += field_line_offset;
    return std::make_tuple(std::move(state), field_line_offset, true);
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_body state, std::span<const std::byte> byte_buffer) {
    ASSERT(state.content_length > state.body.size());

    const std::size_t content_length_left = state.content_length - state.body.size();
    const std::size_t limited_byte_buffer_size = std::min(content_length_left, byte_buffer.size());
    const auto limited_byte_buffer = byte_buffer.subspan(0, limited_byte_buffer_size);
    state.body.insert(state.body.end(), limited_byte_buffer.begin(), limited_byte_buffer.end());

    if (state.body.size() < state.content_length) {
        return std::make_tuple(std::move(state), limited_byte_buffer_size, true);
    }

    ASSERT(state.body.size() == state.content_length);
    return std::make_tuple(deserialize_request_state_complete{ std::move(state) }, limited_byte_buffer_size, true);
}

std::tuple<deserialize_request_state, std::size_t, bool>
    deserialize_request_process(deserialize_request_state_chunked_body state, std::span<const std::byte> byte_buffer) {
    // TODO
    return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
}

std::tuple<deserialize_request_state, std::size_t, bool> deserialize_request_process(
    deserialize_request_state_trailing_fields state,
    std::span<const std::byte> byte_buffer
) {
    // TODO
    return deserialize_request_process_complete_err(status_type::BAD_REQUEST);
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
