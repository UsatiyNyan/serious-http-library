//
// Created by usatiynyan.
//
// HTTP-message   = start-line CRLF
//                 *( field-line CRLF )
//                 CRLF
//                 [ message-body ]
//
// start-line     = request-line
// request-line   = method SP request-target SP HTTP-version
//

#include "sl/http/v1/deserialize/message.hpp"

#include "sl/http/v1/deserialize/target.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <sl/meta/enum/from_string.hpp>
#include <sl/meta/match/overloaded.hpp>

#include <charconv>
#include <variant>

namespace sl::http::v1 {

meta::unique_function<meta::maybe<status_type>(std::span<const std::byte> input)>
    make_parse_request(parse_config config) {
    return [m = detail::parse_machine{ std::move(config), /*is_request=*/true }] //
        (std::span<const std::byte> input) mutable { return m.parse(input); };
}

meta::unique_function<meta::maybe<status_type>(std::span<const std::byte> input)>
    make_parse_response(parse_config config) {
    return [m = detail::parse_machine{ std::move(config), /*is_request=*/false }] //
        (std::span<const std::byte> input) mutable { return m.parse(input); };
}

namespace detail {

meta::maybe<status_type> parse_machine::parse(std::span<const std::byte> input) & {
    if (remainder_.view().empty()) { // less allocations and copying
        while (!input.empty()) {
            const auto result = parse_impl(input);
            if (!result.has_value()) {
                return result.error();
            }
            const std::size_t offset = result.value();
            if (offset == parse_ok::continue_token) {
                continue;
            }
            input = input.subspan(offset);
            if (offset == 0) {
                break;
            }
        }
    }

    std::ignore = remainder_.merge(input);

    while (true) {
        const auto result = parse_impl(remainder_.view());
        if (!result.has_value()) {
            return result.error();
        }
        const std::size_t offset = result.value();
        if (offset == parse_ok::continue_token) {
            continue;
        }
        remainder_.add_offset(offset);
        if (offset == 0) {
            break;
        }
    }

    return meta::null;
}

meta::result<std::size_t, status_type> parse_machine::parse_impl(std::span<const std::byte> input) & {
    return std::visit([&](const auto& state) { return parse_impl(output_, state, config_, input); }, state_)
        .map([&](parse_ok ok) -> std::size_t {
            state_ = std::move(ok.state);

            if (auto* state = std::get_if<parse_state_chunked_body>(&state_)) {
                if (auto* chunked_state = std::get_if<parse_state_chunked_body_complete>(state)) {
                    config_.chunk_cb(
                        message_chunk{
                            .message = output_,
                            .chunk_ext = std::move(chunked_state->chunk_ext),
                            .chunk = chunked_state->chunk,
                        }
                    );
                }
            }

            if (auto* state = std::get_if<parse_state_complete>(&state_)) {
                message_type output;
                output.start_line = std::visit(
                    meta::overloaded{
                        [](const request_line_type&) -> start_line_type { return request_line_type{}; },
                        [](const response_line_type&) -> start_line_type { return response_line_type{}; },
                    },
                    output_.start_line
                );
                config_.message_cb(std::exchange(output_, {}));
            }

            return ok.offset;
        });
}

meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    DEBUG_ASSERT(std::holds_alternative<request_line_type>(output.start_line));
    return std::visit([&](const auto& a_state) { return parse_impl(output, a_state, config, input); }, state);
}

// method SP request-target SP HTTP-version CRLF
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_request state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    return std::visit([&](const auto& a_state) { return parse_impl(output, a_state, config, input); }, state);
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_request_version state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    constexpr std::size_t method_max_length = enum_max_str_length<method_type>();

    const auto method_result = try_find(input_str, tokens::SP, method_max_length);
    if (!method_result.has_value()) {
        const auto& method_err = method_result.error();
        if (method_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::NOT_IMPLEMENTED);
        }
        DEBUG_ASSERT(method_err == find_err::NOT_FOUND);
        return parse_ok::stop(state);
    }

    const auto& [method_str, method_offset] = method_result.value();
    const auto method = meta::enum_from_str<method_type>(method_str);
    if (method == method_type::ENUM_END) {
        return meta::err(status_type::BAD_REQUEST);
    }

    std::get<request_line_type>(output.start_line).method = method;
    return parse_ok{
        .state = parse_state_start_line_request{ parse_state_start_line_request_target{} },
        .offset = method_offset,
    };
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_request_target state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    const auto target_result = try_find(input_str, tokens::SP, config.max_target_size);
    if (!target_result.has_value()) {
        const auto& target_err = target_result.error();
        if (target_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::URI_TOO_LONG);
        }
        DEBUG_ASSERT(target_err == find_err::NOT_FOUND);
        return parse_ok::stop(state);
    }
    const auto& [target_str, target_offset] = target_result.value();

    auto maybe_target = parse_target(target_str);
    if (!maybe_target.has_value()) {
        return meta::err(status_type::BAD_REQUEST);
    }

    std::get<request_line_type>(output.start_line).target = std::move(maybe_target).value();
    return parse_ok{
        .state = parse_state_start_line_request{ parse_state_start_line_request_version{} },
        .offset = target_offset,
    };
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_request_method state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    constexpr std::size_t version_max_length = enum_max_str_length<version_type>();
    const auto version_result = try_find(input_str, tokens::CRLF, version_max_length);
    if (!version_result.has_value()) {
        const auto& version_err = version_result.error();
        if (version_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(version_err == find_err::NOT_FOUND);
        return parse_ok::stop(state);
    }

    const auto& [version_str, version_offset] = version_result.value();
    const auto version = meta::enum_from_str<version_type>(version_str);
    if (version == version_type::ENUM_END) {
        return meta::err(status_type::BAD_REQUEST);
    }

    std::get<request_line_type>(output.start_line).version = version;
    return parse_ok{
        .state = parse_state_fields{},
        .offset = version_offset,
    };
}

// HTTP-version SP status-code SP [ reason-phrase ] CRLF
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_response state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    DEBUG_ASSERT(std::holds_alternative<response_line_type>(output.start_line));
    return std::visit([&](const auto& a_state) { return parse_impl(output, a_state, config, input); }, state);
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_response_version state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    constexpr std::size_t version_max_length = enum_max_str_length<version_type>();
    const auto version_result = try_find(input_str, tokens::SP, version_max_length);
    if (!version_result.has_value()) {
        const auto& version_err = version_result.error();
        if (version_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(version_err == find_err::NOT_FOUND);
        return parse_ok::stop(state);
    }

    const auto& [version_str, version_offset] = version_result.value();
    const auto version = meta::enum_from_str<version_type>(version_str);
    if (version == version_type::ENUM_END) {
        return meta::err(status_type::BAD_REQUEST);
    }

    std::get<response_line_type>(output.start_line).version = version;
    return parse_ok{
        .state = parse_state_start_line_response{ parse_state_start_line_response_status{} },
        .offset = version_offset,
    };
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_response_status state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    constexpr std::size_t status_code_length = 3;

    const auto status_result = try_find(input_str, tokens::SP, status_code_length);
    if (!status_result.has_value()) {
        const auto& status_err = status_result.error();
        if (status_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(status_err == find_err::NOT_FOUND);
        return parse_ok::stop(state);
    }

    const auto& [status_str, status_offset] = status_result.value();
    if (status_str.size() != status_code_length) {
        return meta::err(status_type::BAD_REQUEST);
    }

    std::uint16_t status_code = 0;
    const auto conv_result = std::from_chars(status_str.begin(), status_str.end(), status_code);
    if (conv_result.ec != std::error_code{} || conv_result.ptr != status_str.end()) {
        return meta::err(status_type::BAD_REQUEST);
    }

    std::get<response_line_type>(output.start_line).status = static_cast<status_type>(status_code);
    return parse_ok{
        .state = parse_state_start_line_response{ parse_state_start_line_response_reason{} },
        .offset = status_offset,
    };
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_start_line_response_reason state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    const auto reason_result = try_find(input_str, tokens::CRLF, config.max_reason_size);
    if (!reason_result.has_value()) {
        const auto& reason_err = reason_result.error();
        if (reason_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(reason_err == find_err::NOT_FOUND);
        return parse_ok::stop(state);
    }

    const auto& [reason_str, reason_offset] = reason_result.value();
    std::get<response_line_type>(output.start_line).reason = reason_str;
    return parse_ok{
        .state = parse_state_fields{},
        .offset = reason_offset,
    };
}

// *( field-line CRLF ) CRLF
// field-line   = field-name ":" OWS field-value OWS
// OWS = *(SP / HTAB)
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_fields state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    return parse_impl(output, std::variant<parse_state_fields, parse_state_trailing_fields>(state), config, input);
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_trailing_fields state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    return parse_impl(output, std::variant<parse_state_fields, parse_state_trailing_fields>(state), config, input);
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    std::variant<parse_state_fields, parse_state_trailing_fields> state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    const std::size_t consumed_bytes = std::visit([](const auto& s) { return s.consumed_bytes; }, state);
    const auto field_line_result = try_find(input_str, tokens::CRLF, config.max_field_size - consumed_bytes);
    if (!field_line_result.has_value()) {
        const auto& field_line_err = field_line_result.error();
        if (field_line_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::CONTENT_TOO_LARGE);
        }
        DEBUG_ASSERT(field_line_err == find_err::NOT_FOUND);
        return std::visit([](auto s) { return parse_ok::stop(s); }, state);
    }
    const auto& [field_line, field_line_offset] = field_line_result.value();
    std::visit([field_line_offset](auto& s) { s.consumed_bytes += field_line_offset; }, state);

    if (!field_line.empty()) {
        // not limiting by max_size since field_line_result is already limited
        const auto field_kv_result = try_find_unlimited(field_line, ":");
        if (!field_kv_result.has_value()) {
            return meta::err(status_type::BAD_REQUEST);
        }
        const auto& [field_key, field_offset] = field_kv_result.value();
        constexpr auto strip = [](std::string_view x) {
            x = strip_prefix_while(x, tokens::is_ws);
            x = strip_suffix_while(x, tokens::is_ws);
            return x;
        };
        const auto field_value = strip(field_line.substr(field_offset));

        const auto field_key_lower = to_lowercase(field_key);
        const auto [field_kv_it, field_kv_is_emplaced] =
            output.fields.try_emplace(field_key_lower, std::string{ field_value });
        if (!field_kv_is_emplaced) {
            field_kv_it.value() += ", ";
            field_kv_it.value() += field_value;
        }

        return std::visit(
            [field_line_offset](auto s) {
                return parse_ok{
                    .state = s,
                    .offset = field_line_offset,
                };
            },
            state
        );
    }

    // detected last CRLF
    DEBUG_ASSERT(field_line.empty());

    return std::visit(
        meta::overloaded{
            [&](parse_state_fields) {
                return parse_state_fields_finalize(output, config).map([field_line_offset](parse_state s) {
                    return parse_ok{ .state = s, .offset = field_line_offset };
                });
            },
            [field_line_offset](parse_state_trailing_fields) -> meta::result<parse_ok, status_type> {
                return parse_ok{ .state = parse_state_complete{}, .offset = field_line_offset };
            },
        },
        state
    );
}
meta::result<parse_state, status_type>
    parse_machine::parse_state_fields_finalize(message_type& output, const parse_config& config) {
    const auto find_field = [&fields = output.fields](std::string_view k) -> meta::maybe<std::string_view> {
        DEBUG_ASSERT(is_lowercase(k));
        auto it = fields.find(k);
        if (it == fields.end()) {
            return meta::null;
        }
        return std::string_view{ it.value() };
    };
    constexpr auto check_chunked_field = [](std::string_view transfer_encodings) {
        constexpr std::string_view needle = "chunked";
        const auto strip_ws = [](std::string_view s) {
            return strip_suffix_while(strip_prefix_while(s, tokens::is_ws), tokens::is_ws);
        };
        transfer_encodings = strip_ws(transfer_encodings);
        while (transfer_encodings.size() >= needle.size()) {
            const auto result = try_find_split_unlimited(transfer_encodings, ",");
            if (strip_ws(result.head) == needle) {
                return true;
            }
            if (!result.tail.has_value()) {
                break;
            }
            transfer_encodings = strip_ws(result.tail.value());
        }
        return false;
    };
    constexpr auto parse_content_length = [](std::string_view content_length_str) -> meta::maybe<std::size_t> {
        std::size_t content_length = 0;
        const auto conv_result = std::from_chars(content_length_str.begin(), content_length_str.end(), content_length);
        if (conv_result.ec != std::error_code{} || conv_result.ptr != content_length_str.end()) {
            return meta::null;
        }
        return content_length;
    };

    const auto maybe_transfer_encodings = find_field("transfer-encoding");
    const auto maybe_content_length_str = find_field("content-length");
    const bool is_chunked = maybe_transfer_encodings.map(check_chunked_field).value_or(false);

    if (is_chunked) {
        if (maybe_content_length_str.has_value()) {
            // TODO: or is it?
            return meta::err(status_type::BAD_REQUEST);
        }
        return parse_state_chunked_body{ parse_state_chunked_body_empty{} };
    }

    const auto maybe_content_length = maybe_content_length_str.and_then(parse_content_length);
    const auto content_length = maybe_content_length.value_or(0);
    if (content_length == 0) {
        return parse_state_complete{};
    }

    if (content_length > config.max_body_size) {
        return meta::err(status_type::CONTENT_TOO_LARGE);
    }

    return parse_state_body{ .content_length = content_length };
}

meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_body state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    DEBUG_ASSERT(state.content_length > output.body.size());

    const std::size_t content_length_left = state.content_length - output.body.size();
    const std::size_t limited_byte_buffer_size = std::min(content_length_left, input.size());
    const auto limited_byte_buffer = input.subspan(0, limited_byte_buffer_size);
    output.body.insert(output.body.end(), limited_byte_buffer.begin(), limited_byte_buffer.end());

    if (output.body.size() < state.content_length) {
        return parse_ok{ .state = state, .offset = limited_byte_buffer_size };
    }

    DEBUG_ASSERT(output.body.size() == state.content_length);
    return parse_ok{ .state = parse_state_complete{}, .offset = limited_byte_buffer_size };
}

meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_chunked_body state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    return std::visit([&](const auto& a_state) { return parse_impl(output, a_state, config, input); }, state);
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_chunked_body_empty state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const auto input_str = buffer_byte_to_str(input);
    const auto chunk_line_result = try_find(input_str, tokens::CRLF, config.max_chunk_line_size);
    if (!chunk_line_result.has_value()) {
        const auto chunk_line_err = chunk_line_result.error();
        if (chunk_line_err == find_err::MAX_SIZE_EXCEEDED) {
            return meta::err(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(chunk_line_err == find_err::NOT_FOUND);
        return parse_ok::stop(parse_state_chunked_body{ state });
    }
    const auto& chunk_line_ok = chunk_line_result.value();

    const auto split_result = try_find_split_unlimited(chunk_line_ok.value, ";");

    const auto chunk_size_str = strip_suffix_while(split_result.head, tokens::is_ws); // BWS
    if (chunk_size_str.size() > config.max_chunk_size_size) {
        return meta::err(status_type::BAD_REQUEST);
    }
    const auto maybe_chunk_size = [chunk_size_str]() -> meta::maybe<std::uint32_t> {
        std::uint32_t chunk_size = 0;
        const auto conv_result = std::from_chars(chunk_size_str.begin(), chunk_size_str.end(), chunk_size, 16);
        if (conv_result.ec != std::error_code{} || conv_result.ptr != chunk_size_str.end()) {
            return meta::null;
        }
        return chunk_size;
    }();
    if (!maybe_chunk_size.has_value()) {
        return meta::err(status_type::BAD_REQUEST);
    }
    const std::uint32_t chunk_size = maybe_chunk_size.value();

    const auto chunk_ext = split_result //
                               .tail
                               .map([](std::string_view x) { return strip_prefix_while(x, tokens::is_ws); }) // BWS
                               .value_or(std::string_view{});

    if (chunk_size == 0) { // last-chunk
        return parse_ok{
            .state = parse_state_chunked_body{ parse_state_chunked_body_complete{
                .chunk_ext{ chunk_ext },
                .chunk{},
            } },
            .offset = chunk_line_ok.offset,
        };
    }

    return parse_ok{
        .state = parse_state_chunked_body{ parse_state_chunked_body_line{
            .chunk_ext{ chunk_ext },
            .chunk_size = chunk_size,
        } },
        .offset = chunk_line_ok.offset,
    };
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_chunked_body_line state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    const std::uint32_t chunk_size = state.chunk_size;
    const std::size_t offset = chunk_size + tokens::CRLF.size();
    if (input.size() < offset) {
        return parse_ok::stop(parse_state_chunked_body{ state });
    }

    if (const std::string_view crlf_expected = buffer_byte_to_str(input.subspan(chunk_size, tokens::CRLF.size()));
        crlf_expected != tokens::CRLF) {
        return meta::err(status_type::BAD_REQUEST);
    }

    return parse_ok{
        .state = parse_state_chunked_body{ parse_state_chunked_body_complete{
            .chunk_ext = std::move(state.chunk_ext),
            .chunk = input.subspan(0, chunk_size),
        } },
        .offset = offset,
    };
}
meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_chunked_body_complete state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    if (state.chunk.empty()) { // detected last-chunk
        return parse_ok{
            .state = parse_state_trailing_fields{},
            .offset = parse_ok::continue_token,
        };
    } else {
        return parse_ok{
            .state = parse_state_chunked_body{ parse_state_chunked_body_empty{} },
            .offset = parse_ok::continue_token,
        };
    }
}

meta::result<parse_ok, status_type> parse_machine::parse_impl(
    message_type& output,
    parse_state_complete state,
    const parse_config& config,
    std::span<const std::byte> input
) {
    return parse_ok{
        .state = std::visit(
            meta::overloaded{
                [](const request_line_type&) { return parse_state_start_line{ parse_state_start_line_request{} }; },
                [](const response_line_type&) { return parse_state_start_line{ parse_state_start_line_response{} }; },
            },
            output.start_line
        ),
        .offset = parse_ok::continue_token,
    };
}

} // namespace detail
} // namespace sl::http::v1
