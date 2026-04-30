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

#include "sl/http/v1/deserialize/request.hpp"

#include "sl/http/v1/detail/strings.hpp"
#include "sl/http/v1/detail/target.hpp"

#include <sl/meta/enum/from_string.hpp>
#include <sl/meta/match/overloaded.hpp>

#include <charconv>
#include <variant>

namespace sl::http::v1::deserialize {

exec::async_gen<message_chunk, io_result<message_result>>
    request(exec::async_gen<std::span<const std::byte>, std::error_code> input, const config_type& config) {

    detail::state s;
    message_type output;

    const auto do_parse = [&](
                              auto get_buffer, auto cond, auto add_offset
                          ) -> exec::async_gen<message_chunk, meta::result<meta::unit, status_type>> {
        while (cond()) {
            auto* const chunked_state = std::get_if<detail::state_chunked_body>(&s);
            if (chunked_state == nullptr) {
                const auto [new_state_or_err, offset] = detail::parse_request(output, s, get_buffer(), config);

                if (!new_state_or_err.has_value()) {
                    co_return meta::err(new_state_or_err.error());
                }
                s = new_state_or_err.value();

                if (offset == 0) {
                    break;
                }
                add_offset(offset);
            } else {
                const auto [new_chunked_state_or_err, offset] =
                    detail::parse_chunk(output, std::move(*chunked_state), get_buffer());

                if (!new_chunked_state_or_err.has_value()) {
                    co_return meta::err(new_chunked_state_or_err.error());
                }
                const auto new_chunked_state = new_chunked_state_or_err.value();

                if (auto* const a_chunked_complete_state =
                        std::get_if<detail::state_chunked_body_complete>(&new_chunked_state);
                    a_chunked_complete_state != nullptr) {

                    co_yield message_chunk{
                        .message = output,
                        .chunk_ext = std::move(a_chunked_complete_state->chunk_ext),
                        .chunk = a_chunked_complete_state->chunk,
                    };

                    if (a_chunked_complete_state->chunk.empty()) { // detected last-chunk
                        s = detail::state_trailing_fields{};
                    } else {
                        s = detail::state_chunked_body{ detail::state_chunked_body_empty{} };
                    }
                } else {
                    s = new_chunked_state_or_err.value();
                }

                if (offset == 0) {
                    break;
                }
                add_offset(offset);
            }
        }

        co_return meta::unit{};
    };

    detail::remainder_buffer<> remainder;
    while (const auto maybe_byte_buffer = co_await input) {
        auto byte_buffer = maybe_byte_buffer.value();

        if (remainder.view().empty()) { // less allocations and copying
            auto asyncgen = do_parse(
                [&] { return byte_buffer; },
                [&] { return !byte_buffer.empty(); },
                [&](std::size_t offset) { byte_buffer = byte_buffer.subspan(offset); }
            );
            while (true) {
                auto intermediate = co_await asyncgen;
                if (!intermediate.has_value()) {
                    break;
                }
                co_yield intermediate.value();
            }
            const auto result = std::move(asyncgen).result_or_throw();
            if (!result.has_value()) {
                co_return message_result{ meta::err(result.error()) };
            }
        }

        std::ignore = remainder.merge(byte_buffer);

        auto asyncgen = do_parse(
            [&] { return remainder.view(); },
            [] { return true; },
            [&](std::size_t offset) { remainder.add_offset(offset); }
        );
        while (true) {
            auto intermediate = co_await asyncgen;
            if (!intermediate.has_value()) {
                break;
            }
            co_yield intermediate.value();
        }
        const auto result = std::move(asyncgen).result_or_throw();
        if (!result.has_value()) {
            co_return message_result{ meta::err(result.error()) };
        }

        if (std::holds_alternative<detail::state_complete>(s)) {
            co_return message_result{ std::move(output) };
        }
    }

    co_return meta::err(std::move(input).result_or_throw());
}

namespace detail {

parse_result<meta::result<state, status_type>>
    parse_request(message_type& output, state s, std::span<const std::byte> byte_buffer, const config_type& config) {
    const auto str_buffer = buffer_byte_to_str(byte_buffer);

    return std::visit(
        meta::overloaded{
            [](state_chunked_body a_s) { return make_parse_stop(a_s); },
            [](state_complete a_s) { return make_parse_stop(a_s); },
            [&](state_body a_s) { return parse_part(output, a_s, byte_buffer); },
            [&](request_state_version a_s) { return parse_part(output, state_fields{}, str_buffer, config); },
            [&](state_fields a_s) { return parse_part(output, a_s, str_buffer, config); },
            [&](state_trailing_fields a_s) { return parse_part(output, a_s, str_buffer, config); },
            [&](auto a_s) { return parse_part(output, a_s, str_buffer); },
        },
        s
    );
}

// method SP
parse_result<meta::result<state, status_type>>
    parse_part(message_type& output, state_empty s, std::string_view buffer) {
    constexpr std::size_t method_max_length = enum_max_str_length<method_type>();

    const auto method_result = try_find(buffer, tokens::SP, method_max_length);
    if (!method_result.has_value()) {
        const auto& method_err = method_result.error();
        if (method_err == find_err::MAX_SIZE_EXCEEDED) {
            return make_parse_stop(status_type::NOT_IMPLEMENTED);
        }
        DEBUG_ASSERT(method_err == find_err::NOT_FOUND);
        return make_parse_stop(s);
    }

    const auto& [method_str, method_offset] = method_result.value();
    const auto method = meta::enum_from_str<method_type>(method_str);
    if (method == method_type::ENUM_END) {
        return make_parse_stop(status_type::BAD_REQUEST);
    }

    output.method = method;
    return make_parse_more(request_state_method{}, method_offset);
}

// request-target SP
parse_result<meta::result<state, status_type>>
    parse_part(message_type& output, request_state_method s, std::string_view buffer) {
    // recommended as per https://www.rfc-editor.org/rfc/rfc9112.html#name-request-line
    constexpr std::size_t target_max_length = 8000;

    const auto target_result = try_find(buffer, tokens::SP, target_max_length);
    if (!target_result.has_value()) {
        const auto& target_err = target_result.error();
        if (target_err == find_err::MAX_SIZE_EXCEEDED) {
            return make_parse_stop(status_type::URI_TOO_LONG);
        }
        DEBUG_ASSERT(target_err == find_err::NOT_FOUND);
        return make_parse_stop(s);
    }
    const auto& [target_str, target_offset] = target_result.value();

    auto maybe_target = deserialize::target(target_str);
    if (!maybe_target.has_value()) {
        return make_parse_stop(status_type::BAD_REQUEST);
    }

    output.target = std::move(maybe_target).value();
    return make_parse_more(request_state_target{}, target_offset);
}

// HTTP-version CRLF
parse_result<meta::result<state, status_type>>
    parse_part(message_type& output, request_state_target s, std::string_view buffer) {
    constexpr std::size_t version_max_length = enum_max_str_length<version_type>();

    const auto version_result = try_find(buffer, tokens::CRLF, version_max_length);
    if (!version_result.has_value()) {
        const auto& version_err = version_result.error();
        if (version_err == find_err::MAX_SIZE_EXCEEDED) {
            return make_parse_stop(status_type::BAD_REQUEST);
        }
        DEBUG_ASSERT(version_err == find_err::NOT_FOUND);
        return make_parse_stop(s);
    }

    const auto& [version_str, version_offset] = version_result.value();
    const auto version = meta::enum_from_str<version_type>(version_str);
    if (version == version_type::ENUM_END) {
        return make_parse_stop(status_type::BAD_REQUEST);
    }

    output.version = version;
    return make_parse_more(request_state_version{}, version_offset);
}

// *( field-line CRLF ) CRLF
// field-line   = field-name ":" OWS field-value OWS
// OWS = *(SP / HTAB)
parse_result<meta::result<state, status_type>> parse_part(
    message_type& output,
    std::variant<state_fields, state_trailing_fields> s,
    std::string_view buffer,
    const config_type& config
) {
    const std::size_t consumed_bytes = std::visit([](const auto& a_s) { return a_s.consumed_bytes; }, s);
    const auto field_line_result = try_find(buffer, tokens::CRLF, config.max_field_size - consumed_bytes);
    if (!field_line_result.has_value()) {
        const auto& field_line_err = field_line_result.error();
        if (field_line_err == find_err::MAX_SIZE_EXCEEDED) {
            return make_parse_stop(status_type::CONTENT_TOO_LARGE);
        }
        DEBUG_ASSERT(field_line_err == find_err::NOT_FOUND);
        return std::visit([](auto& a_s) { return make_parse_stop(a_s); }, s);
    }
    const auto& [field_line, field_line_offset] = field_line_result.value();

    if (field_line.empty()) { // detected last CRLF
        return std::visit(
            meta::overloaded{
                [&](state_fields a_s) {
                    return parse_result<meta::result<state, status_type>>::more(
                        parse_fields_finalize(output, a_s.consumed_bytes, config), field_line_offset
                    );
                },
                [&](state_trailing_fields) { return make_parse_more(state_complete{}, field_line_offset); },
            },
            s
        );
    }

    // not limiting by max_size since field_line_result is already limited
    const auto field_kv_result = try_find_unlimited(field_line, ":");
    if (!field_kv_result.has_value()) {
        return make_parse_stop(status_type::BAD_REQUEST);
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
        [field_line_offset](auto& a_s) {
            a_s.consumed_bytes += field_line_offset;
            return make_parse_more(a_s, field_line_offset);
        },
        s
    );
}

meta::result<state, status_type>
    parse_fields_finalize(message_type& output, std::size_t fields_consumed_bytes, const config_type& config) {
    const auto find_field = [&fields = output.fields](std::string_view k) -> meta::maybe<std::string_view> {
        DEBUG_ASSERT(is_lowercase(k));
        auto it = fields.find(k);
        if (it == fields.end()) {
            return meta::null;
        }
        return std::string_view{ it.value() };
    };

    const auto maybe_transfer_encodings = find_field("transfer-encoding");
    const auto maybe_content_length_str = find_field("content-length");

    const bool is_chunked = maybe_transfer_encodings
                                .map([](std::string_view transfer_encodings) {
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
                                })
                                .value_or(false);

    if (is_chunked) {
        if (maybe_content_length_str.has_value()) {
            // TODO: or is it?
            return meta::err(status_type::BAD_REQUEST);
        }

        return state_chunked_body{ detail::state_chunked_body_empty{} };
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

    const auto content_length = maybe_content_length.value_or(0);
    if (content_length == 0) {
        return state_complete{};
    }

    if (content_length > config.max_body_size) {
        return meta::err(status_type::CONTENT_TOO_LARGE);
    }

    return state_body{ .content_length = content_length };
}

parse_result<meta::result<state, status_type>>
    parse_part(message_type& output, state_body s, std::span<const std::byte> buffer) {
    ASSERT(s.content_length > output.body.size());

    const std::size_t content_length_left = s.content_length - output.body.size();
    const std::size_t limited_byte_buffer_size = std::min(content_length_left, buffer.size());
    const auto limited_byte_buffer = buffer.subspan(0, limited_byte_buffer_size);
    output.body.insert(output.body.end(), limited_byte_buffer.begin(), limited_byte_buffer.end());

    if (output.body.size() < s.content_length) {
        return make_parse_more(s, limited_byte_buffer_size);
    }

    ASSERT(output.body.size() == s.content_length);
    return make_parse_more(state_complete{}, limited_byte_buffer_size);
}

parse_result<meta::result<state_chunked_body, status_type>>
    parse_chunk(message_type& output, state_chunked_body s, std::span<const std::byte> buffer) {
    return std::visit(
        meta::overloaded{
            [&buffer](state_chunked_body_empty a_s) {
                constexpr std::size_t max_chunk_size_size = 8;
                constexpr auto max_chunk_line_size = max_chunk_size_size + 8 * 1024; // 8B + 8KiB

                const auto chunk_line_result = try_find(buffer_byte_to_str(buffer), tokens::CRLF, max_chunk_line_size);
                if (!chunk_line_result.has_value()) {
                    const auto chunk_line_err = chunk_line_result.error();
                    if (chunk_line_err == find_err::MAX_SIZE_EXCEEDED) {
                        return make_parse_chunk_stop(status_type::BAD_REQUEST);
                    }
                    DEBUG_ASSERT(chunk_line_err == find_err::NOT_FOUND);
                    return make_parse_chunk_stop(std::move(a_s));
                }
                const auto& chunk_line_ok = chunk_line_result.value();

                const auto split_result = try_find_split_unlimited(chunk_line_ok.value, ";");

                const auto chunk_size_str = strip_suffix_while(split_result.head, tokens::is_ws); // BWS
                if (chunk_size_str.size() > max_chunk_size_size) {
                    return make_parse_chunk_stop(status_type::BAD_REQUEST);
                }
                const auto maybe_chunk_size = [chunk_size_str]() -> meta::maybe<std::uint32_t> {
                    std::uint32_t chunk_size = 0;
                    const auto conv_result =
                        std::from_chars(chunk_size_str.begin(), chunk_size_str.end(), chunk_size, 16);
                    if (conv_result.ec != std::error_code{} || conv_result.ptr != chunk_size_str.end()) {
                        return meta::null;
                    }
                    return chunk_size;
                }();
                if (!maybe_chunk_size.has_value()) {
                    return make_parse_chunk_stop(status_type::BAD_REQUEST);
                }
                const std::uint32_t chunk_size = maybe_chunk_size.value();

                const auto chunk_ext =
                    split_result //
                        .tail
                        .map([](std::string_view x) { return strip_prefix_while(x, tokens::is_ws); }) // BWS
                        .value_or(std::string_view{});

                if (chunk_size == 0) { // last-chunk
                    return make_parse_chunk_more(
                        state_chunked_body_complete{
                            .chunk_ext{ chunk_ext },
                            .chunk{},
                        },
                        chunk_line_ok.offset
                    );
                }

                return make_parse_chunk_more(
                    state_chunked_body_line{
                        .chunk_ext{ chunk_ext },
                        .chunk_size = chunk_size,
                    },
                    chunk_line_ok.offset
                );
            },

            [&buffer](state_chunked_body_line a_s) {
                const std::uint32_t chunk_size = a_s.chunk_size;
                const std::size_t offset = chunk_size + tokens::CRLF.size();
                if (buffer.size() < offset) {
                    return make_parse_chunk_stop(std::move(a_s));
                }

                if (const std::string_view crlf_expected =
                        buffer_byte_to_str(buffer.subspan(chunk_size, tokens::CRLF.size()));
                    crlf_expected != tokens::CRLF) {
                    return make_parse_chunk_stop(status_type::BAD_REQUEST);
                }

                return make_parse_chunk_more(
                    state_chunked_body_complete{
                        .chunk_ext = std::move(a_s.chunk_ext),
                        .chunk = buffer.subspan(0, chunk_size),
                    },
                    offset
                );
            },

            // shouldn't enter this branch
            [](state_chunked_body_complete a_s) { return make_parse_chunk_stop(std::move(a_s)); },
        },
        std::move(s)
    );
}

} // namespace detail
} // namespace sl::http::v1::deserialize
