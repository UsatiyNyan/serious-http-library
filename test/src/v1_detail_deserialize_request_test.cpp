//
// Created by usatiynyan.
//

#include "sl/http/v1/deserialize/request.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <gtest/gtest.h>

#include <exception>
#include <fmt/core.h>
#include <sl/exec.hpp>
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

class DeserializeRequestTest : public ::testing::Test {
protected:
    exec::async_gen<std::span<const std::byte>, std::error_code> full(std::string_view input) {
        co_yield detail::buffer_str_to_byte(input);
        co_return std::error_code{};
    }

    exec::async_gen<std::span<const std::byte>, std::error_code> one_by_one(std::string_view input) {
        for (const std::byte byte : detail::buffer_str_to_byte(input)) {
            std::array<std::byte, 1> buffer{ byte };
            co_yield buffer;
        }
        co_return std::error_code{};
    }

    exec::async<io_result<request_result>> drain_coro(
        exec::async_gen<request_chunk, io_result<request_result>> request_in_progress,
        std::vector<std::byte>* chunk_buffer = nullptr,
        std::vector<std::string>* chunk_ext_buffer = nullptr
    ) {
        while (true) {
            auto maybe_chunk = co_await request_in_progress;
            if (!maybe_chunk.has_value()) {
                break;
            }
            if (chunk_buffer != nullptr) {
                chunk_buffer->insert(chunk_buffer->end(), maybe_chunk->chunk.begin(), maybe_chunk->chunk.end());
            }
            if (chunk_ext_buffer != nullptr) {
                chunk_ext_buffer->emplace_back(maybe_chunk->chunk_ext);
            }
        }
        co_return std::move(request_in_progress).result_or_throw();
    }

    using drain_error = std::variant<meta::unit, std::exception_ptr, std::error_code, status_type>;
    meta::result<request_message, drain_error> drain(
        exec::async_gen<request_chunk, io_result<request_result>> request_in_progress,
        std::vector<std::byte>* chunk_buffer = nullptr,
        std::vector<std::string>* chunk_ext_buffer = nullptr
    ) {
        auto get_result = exec::as_signal(drain_coro(std::move(request_in_progress), chunk_buffer, chunk_ext_buffer))
                          | exec::get<exec::nowait_event>();
        if (!get_result.has_value()) {
            fmt::println("err: unit");
            return meta::err(meta::unit{});
        }
        auto coro_result = std::move(get_result).value();
        if (!coro_result.has_value()) {
            fmt::println("err: exception");
            return meta::err(std::move(coro_result).error());
        }
        auto io_result = std::move(coro_result).value();
        if (!io_result.has_value()) {
            fmt::println("err: io: {}", io_result.error().message());
            return meta::err(std::move(io_result).error());
        }
        auto request_result = std::move(io_result).value();
        if (!request_result.has_value()) {
            fmt::println("err: status: {}", static_cast<std::uint16_t>(request_result.error()));
            return meta::err(std::move(request_result).error());
        }
        return std::move(request_result).value();
    }
};

TEST_F(DeserializeRequestTest, EmptyInput) {
    auto result = drain(request(full("")));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), drain_error(std::error_code{}));
}

TEST_F(DeserializeRequestTest, ValidInput) {
    auto result = drain(request(full("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::GET);
    EXPECT_EQ(result->target, "/");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, InvalidInput) {
    auto result = drain(request(full("INVALID REQUEST")));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), drain_error(status_type::BAD_REQUEST));
}

TEST_F(DeserializeRequestTest, ValidInputWithBody) {
    auto result =
        drain(request(full("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\nHello, World!")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/submit");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeRequestTest, OneByOneInput) {
    auto result = drain(request(one_by_one("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::GET);
    EXPECT_EQ(result->target, "/");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, OneByOneInputWithBody) {
    auto result = drain(
        request(one_by_one("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\nHello, World!"))
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/submit");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeRequestTest, ValidInputWithHeaders) {
    auto result = drain(request(full("GET /resource HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Test\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::GET);
    EXPECT_EQ(result->target, "/resource");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["user-agent"], "Test");
}

TEST_F(DeserializeRequestTest, ValidInputWithChunkedBody) {
    std::vector<std::byte> chunks_buffer;
    auto result = drain(
        request(full(
            "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
        )),
        &chunks_buffer
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/upload");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(chunks_buffer, detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, ValidInputOneByOneWithChunkedBody) {
    std::vector<std::byte> chunks_buffer;
    auto result = drain(
        request(one_by_one(
            "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
        )),
        &chunks_buffer
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/upload");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(chunks_buffer, detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, TransferEncodingNoSpaceAfterComma) {
    // RFC 7230: Transfer-Encoding can have optional whitespace around commas
    // "gzip,chunked" (no space) must work same as "gzip, chunked"
    std::vector<std::byte> chunks_buffer;
    auto result = drain(
        request(full(
            "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: gzip,chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
        )),
        &chunks_buffer
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/upload");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(chunks_buffer, detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, TransferEncodingExtraWhitespace) {
    // RFC 7230: Transfer-Encoding can have extra whitespace around commas
    std::vector<std::byte> chunks_buffer;
    auto result = drain(
        request(full(
            "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding:  gzip ,  chunked \r\n\r\n5\r\nHello\r\n0\r\n\r\n"
        )),
        &chunks_buffer
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/upload");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(chunks_buffer, detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, InvalidInputWithMissingHeaders) {
    // NOT HANDLED FOR NOW
    auto result = drain(request(full("GET / HTTP/1.1\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::GET);
    EXPECT_EQ(result->target, "/");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, ValidInputWithLongBody) {
    std::string long_body(1000, 'a');
    auto result = drain(request(full(fmt::format(
        "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: {}\r\n\r\n{}", long_body.size(), long_body
    ))));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/submit");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->body, detail::buffer_str_to_byte(long_body));
}

TEST_F(DeserializeRequestTest, ValidInputWithMultipleHeaders) {
    auto result =
        drain(request(full("GET /multi HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Test\r\nAccept: */*\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::GET);
    EXPECT_EQ(result->target, "/multi");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["user-agent"], "Test");
    EXPECT_EQ(result->fields["accept"], "*/*");
}

TEST_F(DeserializeRequestTest, ValidInputWithNoBody) {
    auto result = drain(request(full("HEAD /nobody HTTP/1.1\r\nHost: example.com\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::HEAD);
    EXPECT_EQ(result->target, "/nobody");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
}

TEST_F(DeserializeRequestTest, ValidInputWithQueryParameters) {
    auto result = drain(request(full("GET /search?q=test HTTP/1.1\r\nHost: example.com\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::GET);
    EXPECT_EQ(result->target, "/search?q=test");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
}

TEST_F(DeserializeRequestTest, ValidInputWithCustomMethod) {
    // NOT HANDLED
    auto result = drain(request(full("CUSTOM /custom HTTP/1.1\r\nHost: example.com\r\n\r\n")));
    ASSERT_FALSE(result.has_value());
}

TEST_F(DeserializeRequestTest, ValidInputWithTrailingFields) {
    auto result =
        drain(request(full("POST /submit HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "Transfer-Encoding: chunked\r\n\r\n"
                           "5\r\nHello\r\n0\r\n"
                           "Trailer-Header: value\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/submit");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["trailer-header"], "value");
}

TEST_F(DeserializeRequestTest, OneByOneInputWithTrailingFields) {
    auto result =
        drain(request(one_by_one("POST /submit HTTP/1.1\r\n"
                                 "Host: example.com\r\n"
                                 "Transfer-Encoding: chunked\r\n\r\n"
                                 "5\r\nHello\r\n0\r\n"
                                 "Trailer-Header: value\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/submit");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["trailer-header"], "value");
}

TEST_F(DeserializeRequestTest, ValidInputWithChunkExtensions) {
    std::vector<std::byte> chunks_buffer;
    std::vector<std::string> chunk_ext_buffer;
    auto result = drain(
        request(full("POST /upload HTTP/1.1\r\n"
                     "Host: example.com\r\n"
                     "Transfer-Encoding: chunked\r\n\r\n"
                     "5;ext=value\r\nHello\r\n0\r\n\r\n")),
        &chunks_buffer,
        &chunk_ext_buffer
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/upload");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(chunks_buffer, detail::buffer_str_to_byte("Hello"));
    EXPECT_EQ(chunk_ext_buffer.front(), "ext=value");
}

TEST_F(DeserializeRequestTest, OneByOneInputWithChunkExtensions) {
    std::vector<std::byte> chunks_buffer;
    std::vector<std::string> chunk_ext_buffer;
    auto result = drain(
        request(one_by_one("POST /upload HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "Transfer-Encoding: chunked\r\n\r\n"
                           "5;ext=value\r\nHello\r\n0\r\n\r\n")),
        &chunks_buffer,
        &chunk_ext_buffer
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, method_type::POST);
    EXPECT_EQ(result->target, "/upload");
    EXPECT_EQ(result->version, version_type::HTTPv1_1);
    EXPECT_TRUE(result->body.empty());
    EXPECT_EQ(chunks_buffer, detail::buffer_str_to_byte("Hello"));
    EXPECT_EQ(chunk_ext_buffer.front(), "ext=value");
}

TEST_F(DeserializeRequestTest, CaseInsensitiveHeaders) {
    auto result = drain(request(full("GET / HTTP/1.1\r\nHOST: example.com\r\nUser-Agent: Test\r\naccept: */*\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["user-agent"], "Test");
    EXPECT_EQ(result->fields["accept"], "*/*");
}

TEST_F(DeserializeRequestTest, DuplicateHeadersCombined) {
    auto result =
        drain(request(full("GET / HTTP/1.1\r\nHost: example.com\r\nAccept: text/html\r\nAccept: application/json\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["accept"], "text/html, application/json");
}

TEST_F(DeserializeRequestTest, DuplicateHeadersCombinedCaseInsensitive) {
    auto result =
        drain(request(full("GET / HTTP/1.1\r\nHost: example.com\r\nACCEPT: text/html\r\naccept: application/json\r\nAccept: text/plain\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fields["host"], "example.com");
    EXPECT_EQ(result->fields["accept"], "text/html, application/json, text/plain");
}

TEST_F(DeserializeRequestTest, ContentLengthExceedsMaxBodySize) {
    // Use small max_body_size to test DoS protection
    request_config config{ .max_body_size = 10 };
    auto result = drain(request(
        full("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 100\r\n\r\n"),
        config
    ));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), drain_error(status_type::CONTENT_TOO_LARGE));
}

TEST_F(DeserializeRequestTest, ContentLengthWithinMaxBodySize) {
    request_config config{ .max_body_size = 100 };
    auto result = drain(request(
        full("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\nHello, World!"),
        config
    ));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeRequestTest, PipelinedRequestsWithTrailingFields) {
    // Test: pipelined requests where first has trailing fields.
    // Single generator yields bytes one-by-one, shared position tracks consumption.
    //
    // Note: offset bug in detail::parse_request (returning 0 instead of 2 for
    // trailing fields CRLF) doesn't manifest here because each request() owns
    // its remainder_buffer. Generator position tracks actual pulled bytes, which
    // is correct. Bug only affects internal remainder_buffer offset accounting.
    // See TrailingFieldsCRLFOffset test for direct verification of the fix.

    const std::string req1 =
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n"
        "0\r\n"
        "Trailer-Header: value\r\n"
        "\r\n";

    const std::string req2 =
        "GET /page HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    const std::string pipelined = req1 + req2;

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    // Shared position - tracks actual bytes consumed
    std::size_t pos = 0;

    // Generator factory - each call creates new generator reading from current pos
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        while (pos < input_bytes.size()) {
            std::array<std::byte, 1> buf{ input_bytes[pos++] };
            co_yield buf;
        }
        co_return std::error_code{};
    };

    // Parse request 1
    std::vector<std::byte> chunks1;
    auto result1 = drain(request(make_gen()), &chunks1);
    ASSERT_TRUE(result1.has_value()) << "Request 1 should parse";
    EXPECT_EQ(result1->method, method_type::POST);
    EXPECT_EQ(result1->target, "/submit");
    EXPECT_EQ(result1->fields["trailer-header"], "value");
    EXPECT_EQ(chunks1, detail::buffer_str_to_byte("Hello"));

    // Check position after req1 - should be exactly req1.size()
    EXPECT_EQ(pos, req1.size()) << "Position after req1 should match req1 size";

    // Parse request 2
    auto result2 = drain(request(make_gen()));
    ASSERT_TRUE(result2.has_value()) << "Request 2 should parse";
    EXPECT_EQ(result2->method, method_type::GET);
    EXPECT_EQ(result2->target, "/page");

    // All bytes consumed
    EXPECT_EQ(pos, pipelined.size()) << "All bytes should be consumed";
}

TEST_F(DeserializeRequestTest, TrailingFieldsCRLFOffset) {
    // Direct test of detail::parse_request offset when trailing fields end.
    // When empty field line (CRLF) detected in trailing_fields state,
    // returned offset must be 2 to consume the CRLF bytes.
    //
    // Fix: use make_parse_more(request_state_complete{}, field_line_offset)
    // instead of make_parse_stop which returns offset=0.

    request_message output;
    const request_config config;

    // State after "0\r\n" chunk and "Trailer: val\r\n" parsed
    detail::request_state state = detail::request_state_trailing_fields{};

    // Empty field line = just CRLF = end of trailing fields
    const std::string_view input = "\r\nGET / HTTP/1.1\r\n"; // CRLF + next request
    const auto buffer = detail::buffer_str_to_byte(input);

    const auto [result, offset] = detail::parse_request(output, state, buffer, config);

    ASSERT_TRUE(result.has_value()) << "Should parse successfully";
    EXPECT_TRUE(std::holds_alternative<detail::request_state_complete>(result.value()))
        << "Should transition to complete state";

    // Offset must be 2 to consume CRLF, otherwise pipelining breaks
    EXPECT_EQ(offset, 2u) << "Must consume CRLF (2 bytes) for correct pipelining";
}

// === Pipelining Tests ===
// HTTP/1.1 pipelining: multiple requests in single connection, responses in order.
// Key: after parsing one request, unconsumed bytes available for next.

TEST_F(DeserializeRequestTest, PipelinedSimpleRequests) {
    // Two GET requests in single buffer
    const std::string pipelined =
        "GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n"
        "GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    std::size_t pos = 0;
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        while (pos < input_bytes.size()) {
            std::array<std::byte, 1> buf{ input_bytes[pos++] };
            co_yield buf;
        }
        co_return std::error_code{};
    };

    auto result1 = drain(request(make_gen()));
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->method, method_type::GET);
    EXPECT_EQ(result1->target, "/first");

    auto result2 = drain(request(make_gen()));
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->method, method_type::GET);
    EXPECT_EQ(result2->target, "/second");

    EXPECT_EQ(pos, pipelined.size());
}

TEST_F(DeserializeRequestTest, PipelinedWithContentLength) {
    // POST with body + GET
    const std::string pipelined =
        "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nHello"
        "GET /next HTTP/1.1\r\nHost: example.com\r\n\r\n";

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    std::size_t pos = 0;
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        while (pos < input_bytes.size()) {
            std::array<std::byte, 1> buf{ input_bytes[pos++] };
            co_yield buf;
        }
        co_return std::error_code{};
    };

    auto result1 = drain(request(make_gen()));
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->method, method_type::POST);
    EXPECT_EQ(result1->target, "/submit");
    EXPECT_EQ(result1->body, detail::buffer_str_to_byte("Hello"));

    auto result2 = drain(request(make_gen()));
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->method, method_type::GET);
    EXPECT_EQ(result2->target, "/next");

    EXPECT_EQ(pos, pipelined.size());
}

TEST_F(DeserializeRequestTest, PipelinedWithChunkedNoTrailers) {
    // Chunked body (no trailing fields) + GET
    const std::string pipelined =
        "POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n"
        "5\r\nHello\r\n0\r\n\r\n"
        "GET /done HTTP/1.1\r\nHost: example.com\r\n\r\n";

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    std::size_t pos = 0;
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        while (pos < input_bytes.size()) {
            std::array<std::byte, 1> buf{ input_bytes[pos++] };
            co_yield buf;
        }
        co_return std::error_code{};
    };

    std::vector<std::byte> chunks;
    auto result1 = drain(request(make_gen()), &chunks);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->method, method_type::POST);
    EXPECT_EQ(result1->target, "/upload");
    EXPECT_EQ(chunks, detail::buffer_str_to_byte("Hello"));

    auto result2 = drain(request(make_gen()));
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->method, method_type::GET);
    EXPECT_EQ(result2->target, "/done");

    EXPECT_EQ(pos, pipelined.size());
}

TEST_F(DeserializeRequestTest, PipelinedThreeRequests) {
    // Three requests in pipeline
    const std::string pipelined =
        "GET /one HTTP/1.1\r\nHost: a.com\r\n\r\n"
        "GET /two HTTP/1.1\r\nHost: b.com\r\n\r\n"
        "GET /three HTTP/1.1\r\nHost: c.com\r\n\r\n";

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    std::size_t pos = 0;
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        while (pos < input_bytes.size()) {
            std::array<std::byte, 1> buf{ input_bytes[pos++] };
            co_yield buf;
        }
        co_return std::error_code{};
    };

    auto result1 = drain(request(make_gen()));
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->target, "/one");
    EXPECT_EQ(result1->fields["host"], "a.com");

    auto result2 = drain(request(make_gen()));
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->target, "/two");
    EXPECT_EQ(result2->fields["host"], "b.com");

    auto result3 = drain(request(make_gen()));
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3->target, "/three");
    EXPECT_EQ(result3->fields["host"], "c.com");

    EXPECT_EQ(pos, pipelined.size());
}

TEST_F(DeserializeRequestTest, PipelinedFullBuffer) {
    // Both requests arrive in single full() yield (not byte-by-byte)
    // Tests that remainder_buffer correctly tracks unconsumed bytes
    const std::string req1 = "GET /a HTTP/1.1\r\nHost: x\r\n\r\n";
    const std::string req2 = "GET /b HTTP/1.1\r\nHost: y\r\n\r\n";
    const std::string pipelined = req1 + req2;

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    bool yielded = false;
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        if (!yielded) {
            yielded = true;
            co_yield input_bytes;
        }
        co_return std::error_code{};
    };

    // Note: with single full yield, second request() call gets empty generator.
    // This tests behavior when remainder_buffer holds unconsumed data but
    // input generator exhausted. Current impl: second parse fails (no more input).
    //
    // This is expected: caller must track unconsumed bytes externally or
    // use a generator that can provide remainder on subsequent request() calls.

    auto result1 = drain(request(make_gen()));
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->target, "/a");

    // Second call: generator exhausted, no bytes available
    auto result2 = drain(request(make_gen()));
    // Expected: fails because generator yields nothing
    EXPECT_FALSE(result2.has_value());
}

TEST_F(DeserializeRequestTest, PipelinedMixedBodies) {
    // Mix: no body, content-length body, chunked body
    const std::string pipelined =
        "GET /nobody HTTP/1.1\r\nHost: a\r\n\r\n"
        "POST /clbody HTTP/1.1\r\nHost: b\r\nContent-Length: 3\r\n\r\nABC"
        "POST /chunked HTTP/1.1\r\nHost: c\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nXY\r\n0\r\n\r\n";

    std::vector<std::byte> input_bytes(
        detail::buffer_str_to_byte(pipelined).begin(),
        detail::buffer_str_to_byte(pipelined).end()
    );

    std::size_t pos = 0;
    auto make_gen = [&]() -> exec::async_gen<std::span<const std::byte>, std::error_code> {
        while (pos < input_bytes.size()) {
            std::array<std::byte, 1> buf{ input_bytes[pos++] };
            co_yield buf;
        }
        co_return std::error_code{};
    };

    // Request 1: GET, no body
    auto result1 = drain(request(make_gen()));
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->target, "/nobody");
    EXPECT_TRUE(result1->body.empty());

    // Request 2: POST with Content-Length
    auto result2 = drain(request(make_gen()));
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->target, "/clbody");
    EXPECT_EQ(result2->body, detail::buffer_str_to_byte("ABC"));

    // Request 3: POST chunked
    std::vector<std::byte> chunks;
    auto result3 = drain(request(make_gen()), &chunks);
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3->target, "/chunked");
    EXPECT_EQ(chunks, detail::buffer_str_to_byte("XY"));

    EXPECT_EQ(pos, pipelined.size());
}

} // namespace sl::http::v1::deserialize
