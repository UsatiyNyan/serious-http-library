//
// Created by usatiynyan.
//

#include "sl/http/v1/deserialize.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <gtest/gtest.h>

#include <exception>
#include <fmt/core.h>
#include <variant>

namespace std {
bool operator==(const vector<byte>& body, const span<const byte>& expected) {
    return body.size() == expected.size() && equal(body.begin(), body.end(), expected.begin());
}
} // namespace std

namespace sl::meta {
bool operator==(unit, unit) { return true; }
} // namespace sl::meta

namespace sl::http::v1::deserialize {

// Helper to extract request_line from message
const request_line_type& get_request_line(const message_type& msg) {
    const auto* req = std::get_if<request_line_type>(&msg.start_line);
    if (!req) {
        throw std::runtime_error("Expected request_line_type");
    }
    return *req;
}

// Helper to check origin-form target path
std::string_view get_origin_path(const target_type& t) {
    const auto* origin = std::get_if<origin_target_type>(&t);
    if (!origin) {
        throw std::runtime_error("Expected origin_target_type");
    }
    return origin->path;
}

// Stored chunk - copies data since original span lifetime ends with callback
struct stored_chunk {
    std::string chunk_ext;
    std::vector<std::byte> chunk;
};

class DeserializeRequestTest : public ::testing::Test {
protected:
    using drain_error = std::variant<meta::unit, std::exception_ptr, std::error_code, status_type>;

    struct result_type {
        std::vector<stored_chunk> chunks;
        meta::maybe<message_type> message;
        meta::maybe<status_type> error;

        bool has_value() const { return message.has_value(); }
        const message_type& value() const { return message.value(); }
        const message_type& operator*() const { return *message; }
        const message_type* operator->() const { return &*message; }
    };

    // Collect chunk data into vector
    static std::vector<std::byte> collect_chunks(const std::vector<stored_chunk>& chunks) {
        std::vector<std::byte> result;
        for (const auto& chunk : chunks) {
            result.insert(result.end(), chunk.chunk.begin(), chunk.chunk.end());
        }
        return result;
    }

    // Collect chunk extensions
    static std::vector<std::string> collect_chunk_exts(const std::vector<stored_chunk>& chunks) {
        std::vector<std::string> result;
        for (const auto& chunk : chunks) {
            if (!chunk.chunk_ext.empty()) {
                result.push_back(chunk.chunk_ext);
            }
        }
        return result;
    }

    // Helper: deserialize request with full buffer
    result_type drain_request_full(std::string_view input, const deserialize_config& base_config = {}) {
        result_type result;
        deserialize_config config{
            .chunk_cb = [&](message_chunk chunk) {
                result.chunks.push_back(stored_chunk{
                    .chunk_ext = std::move(chunk.chunk_ext),
                    .chunk = std::vector<std::byte>(chunk.chunk.begin(), chunk.chunk.end()),
                });
            },
            .message_cb = [&](message_type msg) { result.message = std::move(msg); },
            .max_body_size = base_config.max_body_size,
            .max_field_size = base_config.max_field_size,
            .max_reason_size = base_config.max_reason_size,
            .max_target_size = base_config.max_target_size,
        };
        auto deserializer = make_deserialize_request(std::move(config));
        result.error = deserializer(detail::buffer_str_to_byte(input));
        return result;
    }

    // Helper: deserialize request byte-by-byte
    result_type drain_request_one_by_one(std::string_view input, const deserialize_config& base_config = {}) {
        result_type result;
        deserialize_config config{
            .chunk_cb = [&](message_chunk chunk) {
                result.chunks.push_back(stored_chunk{
                    .chunk_ext = std::move(chunk.chunk_ext),
                    .chunk = std::vector<std::byte>(chunk.chunk.begin(), chunk.chunk.end()),
                });
            },
            .message_cb = [&](message_type msg) { result.message = std::move(msg); },
            .max_body_size = base_config.max_body_size,
            .max_field_size = base_config.max_field_size,
            .max_reason_size = base_config.max_reason_size,
            .max_target_size = base_config.max_target_size,
        };
        auto deserializer = make_deserialize_request(std::move(config));
        auto bytes = detail::buffer_str_to_byte(input);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            result.error = deserializer(bytes.subspan(i, 1));
            if (result.error.has_value()) {
                return result;
            }
        }
        return result;
    }

    // Helper: deserialize response with full buffer
    result_type drain_response_full(std::string_view input, const deserialize_config& base_config = {}) {
        result_type result;
        deserialize_config config{
            .chunk_cb = [&](message_chunk chunk) {
                result.chunks.push_back(stored_chunk{
                    .chunk_ext = std::move(chunk.chunk_ext),
                    .chunk = std::vector<std::byte>(chunk.chunk.begin(), chunk.chunk.end()),
                });
            },
            .message_cb = [&](message_type msg) { result.message = std::move(msg); },
            .max_body_size = base_config.max_body_size,
            .max_field_size = base_config.max_field_size,
            .max_reason_size = base_config.max_reason_size,
            .max_target_size = base_config.max_target_size,
        };
        auto deserializer = make_deserialize_response(std::move(config));
        result.error = deserializer(detail::buffer_str_to_byte(input));
        return result;
    }

    // Helper: deserialize response byte-by-byte
    result_type drain_response_one_by_one(std::string_view input, const deserialize_config& base_config = {}) {
        result_type result;
        deserialize_config config{
            .chunk_cb = [&](message_chunk chunk) {
                result.chunks.push_back(stored_chunk{
                    .chunk_ext = std::move(chunk.chunk_ext),
                    .chunk = std::vector<std::byte>(chunk.chunk.begin(), chunk.chunk.end()),
                });
            },
            .message_cb = [&](message_type msg) { result.message = std::move(msg); },
            .max_body_size = base_config.max_body_size,
            .max_field_size = base_config.max_field_size,
            .max_reason_size = base_config.max_reason_size,
            .max_target_size = base_config.max_target_size,
        };
        auto deserializer = make_deserialize_response(std::move(config));
        auto bytes = detail::buffer_str_to_byte(input);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            result.error = deserializer(bytes.subspan(i, 1));
            if (result.error.has_value()) {
                return result;
            }
        }
        return result;
    }

    // === Pipelining helpers ===
    // Result type for pipelined requests - collects multiple messages
    struct pipelined_result_type {
        std::vector<message_type> messages;
        std::vector<std::vector<stored_chunk>> chunks_per_message;
        std::vector<stored_chunk> current_chunks;
        meta::maybe<status_type> error;

        std::size_t count() const { return messages.size(); }
    };

    // Helper: deserialize pipelined requests with full buffer
    pipelined_result_type drain_pipelined_requests(std::string_view input, const deserialize_config& base_config = {}) {
        pipelined_result_type result;
        deserialize_config config{
            .chunk_cb = [&](message_chunk chunk) {
                result.current_chunks.push_back(stored_chunk{
                    .chunk_ext = std::move(chunk.chunk_ext),
                    .chunk = std::vector<std::byte>(chunk.chunk.begin(), chunk.chunk.end()),
                });
            },
            .message_cb = [&](message_type msg) {
                result.messages.push_back(std::move(msg));
                result.chunks_per_message.push_back(std::move(result.current_chunks));
                result.current_chunks.clear();
            },
            .max_body_size = base_config.max_body_size,
            .max_field_size = base_config.max_field_size,
            .max_reason_size = base_config.max_reason_size,
            .max_target_size = base_config.max_target_size,
        };
        auto deserializer = make_deserialize_request(std::move(config));
        result.error = deserializer(detail::buffer_str_to_byte(input));
        return result;
    }

    // Helper: deserialize pipelined requests byte-by-byte
    pipelined_result_type drain_pipelined_requests_one_by_one(std::string_view input, const deserialize_config& base_config = {}) {
        pipelined_result_type result;
        deserialize_config config{
            .chunk_cb = [&](message_chunk chunk) {
                result.current_chunks.push_back(stored_chunk{
                    .chunk_ext = std::move(chunk.chunk_ext),
                    .chunk = std::vector<std::byte>(chunk.chunk.begin(), chunk.chunk.end()),
                });
            },
            .message_cb = [&](message_type msg) {
                result.messages.push_back(std::move(msg));
                result.chunks_per_message.push_back(std::move(result.current_chunks));
                result.current_chunks.clear();
            },
            .max_body_size = base_config.max_body_size,
            .max_field_size = base_config.max_field_size,
            .max_reason_size = base_config.max_reason_size,
            .max_target_size = base_config.max_target_size,
        };
        auto deserializer = make_deserialize_request(std::move(config));
        auto bytes = detail::buffer_str_to_byte(input);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            result.error = deserializer(bytes.subspan(i, 1));
            if (result.error.has_value()) {
                return result;
            }
        }
        return result;
    }
};

TEST_F(DeserializeRequestTest, EmptyInput) {
    auto result = drain_request_full("");
    ASSERT_FALSE(result.has_value());
}

TEST_F(DeserializeRequestTest, ValidInput) {
    auto result = drain_request_full("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, InvalidInput) {
    auto result = drain_request_full("INVALID REQUEST");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error, status_type::BAD_REQUEST);
}

TEST_F(DeserializeRequestTest, ValidInputWithBody) {
    auto result =
        drain_request_full("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\nHello, World!");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/submit");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeRequestTest, OneByOneInput) {
    auto result = drain_request_one_by_one("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, OneByOneInputWithBody) {
    auto result =
        drain_request_one_by_one("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\nHello, World!");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/submit");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeRequestTest, ValidInputWithHeaders) {
    auto result = drain_request_full("GET /resource HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Test\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/resource");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("user-agent"), "Test");
}

TEST_F(DeserializeRequestTest, ValidInputWithChunkedBody) {
    auto result = drain_request_full(
        "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/upload");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, ValidInputOneByOneWithChunkedBody) {
    auto result = drain_request_one_by_one(
        "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/upload");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, TransferEncodingNoSpaceAfterComma) {
    // RFC 7230: Transfer-Encoding can have optional whitespace around commas
    // "gzip,chunked" (no space) must work same as "gzip, chunked"
    auto result = drain_request_full(
        "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: "
        "gzip,chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/upload");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, TransferEncodingExtraWhitespace) {
    // RFC 7230: Transfer-Encoding can have extra whitespace around commas
    auto result = drain_request_full(
        "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding:  gzip ,  chunked "
        "\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/upload");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, InvalidInputWithMissingHeaders) {
    // NOT HANDLED FOR NOW
    auto result = drain_request_full("GET / HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, ValidInputWithLongBody) {
    std::string long_body(1000, 'a');
    auto result = drain_request_full(fmt::format(
        "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: {}\r\n\r\n{}", long_body.size(), long_body
    ));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/submit");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte(long_body));
}

TEST_F(DeserializeRequestTest, ValidInputWithMultipleHeaders) {
    auto result =
        drain_request_full("GET /multi HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Test\r\nAccept: */*\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/multi");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("user-agent"), "Test");
    EXPECT_EQ(result->fields.at("accept"), "*/*");
}

TEST_F(DeserializeRequestTest, ValidInputWithNoBody) {
    auto result = drain_request_full("HEAD /nobody HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::HEAD);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/nobody");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
}

TEST_F(DeserializeRequestTest, ValidInputWithQueryParameters) {
    auto result = drain_request_full("GET /search?q=test HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::GET);
    const auto* origin = std::get_if<origin_target_type>(&get_request_line(*result).target);
    ASSERT_NE(origin, nullptr);
    ASSERT_EQ(origin->query.size(), 1);
    EXPECT_EQ(origin->query[0].first, "q");
    EXPECT_EQ(origin->query[0].second, "test");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, ValidInputWithCustomMethod) {
    // NOT HANDLED
    auto result = drain_request_full("CUSTOM /custom HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_FALSE(result.has_value());
}

TEST_F(DeserializeRequestTest, ValidInputWithTrailingFields) {
    auto result = drain_request_full(
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n0\r\n"
        "Trailer-Header: value\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/submit");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("trailer-header"), "value");
}

TEST_F(DeserializeRequestTest, OneByOneInputWithTrailingFields) {
    auto result = drain_request_one_by_one(
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n0\r\n"
        "Trailer-Header: value\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/submit");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("trailer-header"), "value");
}

TEST_F(DeserializeRequestTest, ValidInputWithChunkExtensions) {
    auto result = drain_request_full(
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5;ext=value\r\nHello\r\n0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/upload");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
    EXPECT_EQ(collect_chunk_exts(result.chunks).front(), "ext=value");
}

TEST_F(DeserializeRequestTest, OneByOneInputWithChunkExtensions) {
    auto result = drain_request_one_by_one(
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5;ext=value\r\nHello\r\n0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_request_line(*result).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(*result).target), "/upload");
    EXPECT_EQ(get_request_line(*result).version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
    EXPECT_EQ(collect_chunk_exts(result.chunks).front(), "ext=value");
}

TEST_F(DeserializeRequestTest, CaseInsensitiveHeaders) {
    auto result =
        drain_request_full("GET / HTTP/1.1\r\nHOST: example.com\r\nUser-Agent: Test\r\naccept: */*\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("user-agent"), "Test");
    EXPECT_EQ(result->fields.at("accept"), "*/*");
}

TEST_F(DeserializeRequestTest, DuplicateHeadersCombined) {
    auto result =
        drain_request_full("GET / HTTP/1.1\r\nHost: example.com\r\nAccept: text/html\r\nAccept: application/json\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("accept"), "text/html, application/json");
}

TEST_F(DeserializeRequestTest, DuplicateHeadersCombinedCaseInsensitive) {
    auto result = drain_request_full(
        "GET / HTTP/1.1\r\nHost: example.com\r\nACCEPT: text/html\r\naccept: application/json\r\nAccept: "
        "text/plain\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields.at("host"), "example.com");
    EXPECT_EQ(result->fields.at("accept"), "text/html, application/json, text/plain");
}

TEST_F(DeserializeRequestTest, ContentLengthExceedsMaxBodySize) {
    // Use small max_body_size to test DoS protection
    deserialize_config config{
        .chunk_cb = [](message_chunk) {},
        .message_cb = [](message_type) {},
        .max_body_size = 10,
    };
    auto result =
        drain_request_full("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 100\r\n\r\n", config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error, status_type::CONTENT_TOO_LARGE);
}

TEST_F(DeserializeRequestTest, ContentLengthWithinMaxBodySize) {
    deserialize_config config{
        .chunk_cb = [](message_chunk) {},
        .message_cb = [](message_type) {},
        .max_body_size = 100,
    };
    auto result = drain_request_full(
        "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\nHello, World!", config
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

// Pipelining test disabled - API changed from async_gen to callback-based state machine.
// Pipelined request with trailing fields followed by another request
TEST_F(DeserializeRequestTest, PipelinedRequestsWithTrailingFields) {
    const std::string_view input =
        "POST /first HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Trailers: X-Checksum\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "0\r\n"
        "X-Checksum: abc123\r\n"
        "\r\n"
        "GET /second HTTP/1.1\r\n"
        "\r\n";

    auto result = drain_pipelined_requests(input);
    ASSERT_FALSE(result.error.has_value()) << "Should parse without error";
    ASSERT_EQ(result.count(), 2u) << "Should parse 2 pipelined requests";

    // First request
    EXPECT_EQ(get_request_line(result.messages[0]).method, method_type::POST);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[0]).target), "/first");
    EXPECT_EQ(result.messages[0].fields.at("x-checksum"), "abc123");

    // Second request
    EXPECT_EQ(get_request_line(result.messages[1]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[1]).target), "/second");
}

TEST_F(DeserializeRequestTest, TrailingFieldsCRLFOffset) {
    // Direct test of deserialize_machine::deserialize_impl offset when trailing fields end.
    // When empty field line (CRLF) detected in trailing_fields state,
    // returned offset must be 2 to consume the CRLF bytes.

    message_type output;
    output.start_line = request_line_type{};
    deserialize_config config{
        .chunk_cb = [](message_chunk) {},
        .message_cb = [](message_type) {},
    };

    // State after "0\r\n" chunk and "Trailer: val\r\n" parsed
    detail::deserialize_state_trailing_fields state{};

    // Empty field line = just CRLF = end of trailing fields
    const std::string_view input = "\r\nGET / HTTP/1.1\r\n"; // CRLF + next request
    const auto buffer = detail::buffer_str_to_byte(input);

    const auto result = detail::deserialize_machine::deserialize_impl(output, state, config, buffer);

    ASSERT_TRUE(result.has_value()) << "Should parse successfully";
    EXPECT_TRUE(std::holds_alternative<detail::deserialize_state_complete>(result.value().state))
        << "Should transition to complete state";

    // Offset must be 2 to consume CRLF, otherwise pipelining breaks
    EXPECT_EQ(result.value().offset, 2u) << "Must consume CRLF (2 bytes) for correct pipelining";
}

// === Pipelining Tests ===
// HTTP/1.1 pipelining: multiple requests in single connection, responses in order.

TEST_F(DeserializeRequestTest, PipelinedSimpleRequests) {
    const std::string_view input =
        "GET /first HTTP/1.1\r\n\r\n"
        "GET /second HTTP/1.1\r\n\r\n";

    auto result = drain_pipelined_requests(input);
    ASSERT_FALSE(result.error.has_value());
    ASSERT_EQ(result.count(), 2u);

    EXPECT_EQ(get_request_line(result.messages[0]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[0]).target), "/first");

    EXPECT_EQ(get_request_line(result.messages[1]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[1]).target), "/second");
}

TEST_F(DeserializeRequestTest, PipelinedWithContentLength) {
    const std::string_view input =
        "POST /upload HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello"
        "GET /next HTTP/1.1\r\n\r\n";

    auto result = drain_pipelined_requests(input);
    ASSERT_FALSE(result.error.has_value());
    ASSERT_EQ(result.count(), 2u);

    EXPECT_EQ(get_request_line(result.messages[0]).method, method_type::POST);
    EXPECT_EQ(result.messages[0].body, detail::buffer_str_to_byte("Hello"));

    EXPECT_EQ(get_request_line(result.messages[1]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[1]).target), "/next");
}

TEST_F(DeserializeRequestTest, PipelinedWithChunkedNoTrailers) {
    const std::string_view input =
        "POST /chunked HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "0\r\n"
        "\r\n"
        "GET /after HTTP/1.1\r\n\r\n";

    auto result = drain_pipelined_requests(input);
    ASSERT_FALSE(result.error.has_value());
    ASSERT_EQ(result.count(), 2u);

    EXPECT_EQ(get_request_line(result.messages[0]).method, method_type::POST);
    auto chunks = collect_chunks(result.chunks_per_message[0]);
    EXPECT_EQ(chunks, detail::buffer_str_to_byte("Hello"));

    EXPECT_EQ(get_request_line(result.messages[1]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[1]).target), "/after");
}

TEST_F(DeserializeRequestTest, PipelinedThreeRequests) {
    const std::string_view input =
        "GET /one HTTP/1.1\r\n\r\n"
        "GET /two HTTP/1.1\r\n\r\n"
        "GET /three HTTP/1.1\r\n\r\n";

    auto result = drain_pipelined_requests(input);
    ASSERT_FALSE(result.error.has_value());
    ASSERT_EQ(result.count(), 3u);

    EXPECT_EQ(get_origin_path(get_request_line(result.messages[0]).target), "/one");
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[1]).target), "/two");
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[2]).target), "/three");
}

TEST_F(DeserializeRequestTest, PipelinedOneByOne) {
    // Test pipelining with byte-by-byte feeding
    const std::string_view input =
        "GET /a HTTP/1.1\r\n\r\n"
        "GET /b HTTP/1.1\r\n\r\n";

    auto result = drain_pipelined_requests_one_by_one(input);
    ASSERT_FALSE(result.error.has_value());
    ASSERT_EQ(result.count(), 2u);

    EXPECT_EQ(get_origin_path(get_request_line(result.messages[0]).target), "/a");
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[1]).target), "/b");
}

TEST_F(DeserializeRequestTest, PipelinedMixedBodies) {
    // Mix of no body, content-length, and chunked
    const std::string_view input =
        "GET /nobody HTTP/1.1\r\n\r\n"
        "POST /content HTTP/1.1\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "ABC"
        "POST /chunked HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "2\r\nXY\r\n"
        "0\r\n"
        "\r\n"
        "GET /final HTTP/1.1\r\n\r\n";

    auto result = drain_pipelined_requests(input);
    ASSERT_FALSE(result.error.has_value());
    ASSERT_EQ(result.count(), 4u);

    // First: GET with no body
    EXPECT_EQ(get_request_line(result.messages[0]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[0]).target), "/nobody");
    EXPECT_TRUE(result.messages[0].body.empty());

    // Second: POST with content-length
    EXPECT_EQ(get_request_line(result.messages[1]).method, method_type::POST);
    EXPECT_EQ(result.messages[1].body, detail::buffer_str_to_byte("ABC"));

    // Third: POST with chunked
    EXPECT_EQ(get_request_line(result.messages[2]).method, method_type::POST);
    auto chunks = collect_chunks(result.chunks_per_message[2]);
    EXPECT_EQ(chunks, detail::buffer_str_to_byte("XY"));

    // Fourth: GET
    EXPECT_EQ(get_request_line(result.messages[3]).method, method_type::GET);
    EXPECT_EQ(get_origin_path(get_request_line(result.messages[3]).target), "/final");
}

// === Response Deserialization Tests ===

// Helper to extract response_line from message
const response_line_type& get_response_line(const message_type& msg) {
    const auto* res = std::get_if<response_line_type>(&msg.start_line);
    if (!res) {
        throw std::runtime_error("Expected response_line_type");
    }
    return *res;
}

class DeserializeResponseTest : public DeserializeRequestTest {};

TEST_F(DeserializeResponseTest, EmptyInput) {
    auto result = drain_response_full("");
    ASSERT_FALSE(result.has_value());
}

TEST_F(DeserializeResponseTest, ValidInput200) {
    auto result = drain_response_full("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(get_response_line(*result).reason, "OK");
}

TEST_F(DeserializeResponseTest, ValidInput404) {
    auto result = drain_response_full("HTTP/1.1 404 Not Found\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(get_response_line(*result).status, status_type::NOT_FOUND);
    EXPECT_EQ(get_response_line(*result).reason, "Not Found");
}

TEST_F(DeserializeResponseTest, ValidInput500) {
    auto result = drain_response_full("HTTP/1.1 500 Internal Server Error\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(get_response_line(*result).status, status_type::INTERNAL_SERVER_ERROR);
    EXPECT_EQ(get_response_line(*result).reason, "Internal Server Error");
}

TEST_F(DeserializeResponseTest, HTTP10Version) {
    auto result = drain_response_full("HTTP/1.0 200 OK\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).version, version_type::HTTPv1_0);
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
}

TEST_F(DeserializeResponseTest, EmptyReasonPhrase) {
    // RFC 9112: reason-phrase can be empty
    auto result = drain_response_full("HTTP/1.1 200 \r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(get_response_line(*result).reason, "");
}

TEST_F(DeserializeResponseTest, InvalidVersion) {
    auto result = drain_response_full("HTTP/2.0 200 OK\r\n\r\n");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error, status_type::BAD_REQUEST);
}

TEST_F(DeserializeResponseTest, InvalidStatusCode) {
    auto result = drain_response_full("HTTP/1.1 ABC OK\r\n\r\n");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error, status_type::BAD_REQUEST);
}

TEST_F(DeserializeResponseTest, StatusCodeTooShort) {
    auto result = drain_response_full("HTTP/1.1 20 OK\r\n\r\n");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error, status_type::BAD_REQUEST);
}

TEST_F(DeserializeResponseTest, ValidInputWithBody) {
    auto result = drain_response_full("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeResponseTest, ValidInputWithHeaders) {
    auto result = drain_response_full(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Server: TestServer\r\n"
        "\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields.at("content-type"), "text/html");
    EXPECT_EQ(result->fields.at("server"), "TestServer");
}

TEST_F(DeserializeResponseTest, OneByOneInput) {
    auto result = drain_response_one_by_one("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).version, version_type::HTTPv1_1);
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(get_response_line(*result).reason, "OK");
}

TEST_F(DeserializeResponseTest, OneByOneInputWithBody) {
    auto result = drain_response_one_by_one("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeResponseTest, ChunkedBody) {
    auto result = drain_response_full(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n"
        "6\r\n World\r\n"
        "0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello World"));
}

TEST_F(DeserializeResponseTest, ChunkedBodyOneByOne) {
    auto result = drain_response_one_by_one(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n"
        "0\r\n\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::OK);
    EXPECT_EQ(collect_chunks(result.chunks), detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeResponseTest, ChunkedWithTrailingFields) {
    auto result = drain_response_full(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n"
        "0\r\n"
        "X-Checksum: abc123\r\n"
        "\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields.at("x-checksum"), "abc123");
}

TEST_F(DeserializeResponseTest, UnknownStatusCode) {
    // Non-standard status code (e.g., 299) should still parse
    auto result = drain_response_full("HTTP/1.1 299 Custom Status\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(static_cast<std::uint16_t>(get_response_line(*result).status), 299);
    EXPECT_EQ(get_response_line(*result).reason, "Custom Status");
}

TEST_F(DeserializeResponseTest, ReasonPhraseMaxLength) {
    // Test config.max_reason_size enforcement
    deserialize_config config{
        .chunk_cb = [](message_chunk) {},
        .message_cb = [](message_type) {},
        .max_reason_size = 10,
    };
    auto result = drain_response_full("HTTP/1.1 200 This reason phrase is way too long\r\n\r\n", config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error, status_type::BAD_REQUEST);
}

TEST_F(DeserializeResponseTest, ReasonPhraseWithinLimit) {
    deserialize_config config{
        .chunk_cb = [](message_chunk) {},
        .message_cb = [](message_type) {},
        .max_reason_size = 100,
    };
    auto result = drain_response_full("HTTP/1.1 200 OK\r\n\r\n", config);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).reason, "OK");
}

TEST_F(DeserializeResponseTest, Continue100) {
    auto result = drain_response_full("HTTP/1.1 100 Continue\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::CONTINUE);
    EXPECT_EQ(get_response_line(*result).reason, "Continue");
}

TEST_F(DeserializeResponseTest, NoContent204) {
    auto result = drain_response_full("HTTP/1.1 204 No Content\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::NO_CONTENT);
    EXPECT_TRUE(result->body.empty());
}

TEST_F(DeserializeResponseTest, Redirect301) {
    auto result = drain_response_full(
        "HTTP/1.1 301 Moved Permanently\r\n"
        "Location: https://example.com/new\r\n"
        "\r\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(get_response_line(*result).status, status_type::MOVED_PERMANENTLY);
    EXPECT_EQ(result->fields.at("location"), "https://example.com/new");
}

} // namespace sl::http::v1::deserialize
