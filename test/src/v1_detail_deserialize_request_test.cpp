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

    exec::async<io_result<request_result>>
        drain_coro(exec::async_gen<request_chunk, io_result<request_result>> request_in_progress) {
        while ((co_await request_in_progress).has_value()) {}
        co_return std::move(request_in_progress).result_or_throw();
    }

    using drain_error = std::variant<meta::unit, std::exception_ptr, std::error_code, status_type>;
    meta::result<request_message, drain_error>
        drain(exec::async_gen<request_chunk, io_result<request_result>> request_in_progress) {
        auto get_result = exec::as_signal(drain_coro(std::move(request_in_progress))) | exec::get<exec::nowait_event>();
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
    EXPECT_EQ(result.value().method, method_type::POST);
    EXPECT_EQ(result.value().target, "/submit");
    EXPECT_EQ(result.value().version, version_type::HTTPv1_1);
    EXPECT_EQ(result.value().body, detail::buffer_str_to_byte("Hello, World!"));
}

TEST_F(DeserializeRequestTest, ValidInputWithHeaders) {
    auto result = drain(request(full("GET /resource HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Test\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().method, method_type::GET);
    EXPECT_EQ(result.value().target, "/resource");
    EXPECT_EQ(result.value().version, version_type::HTTPv1_1);
    EXPECT_EQ(result.value().fields["Host"], "example.com");
    EXPECT_EQ(result.value().fields["User-Agent"], "Test");
}

TEST_F(DeserializeRequestTest, ValidInputWithChunkedBody) {
    auto result = drain(request(full("POST /upload HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n")));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().method, method_type::POST);
    EXPECT_EQ(result.value().target, "/upload");
    EXPECT_EQ(result.value().version, version_type::HTTPv1_1);
    EXPECT_EQ(result.value().body, detail::buffer_str_to_byte("Hello"));
}

TEST_F(DeserializeRequestTest, InvalidInputWithMissingHeaders) {
    auto result = drain(request(full("GET / HTTP/1.1\r\n\r\n")));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), drain_error(status_type::BAD_REQUEST));
}

TEST_F(DeserializeRequestTest, ValidInputWithLongBody) {
    std::string long_body(1000, 'a');
    auto result = drain(request(full(fmt::format("POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: {}\r\n\r\n{}", long_body.size(), long_body))));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().method, method_type::POST);
    EXPECT_EQ(result.value().target, "/submit");
    EXPECT_EQ(result.value().version, version_type::HTTPv1_1);
    EXPECT_EQ(result.value().body, detail::buffer_str_to_byte(long_body));
}

} // namespace sl::http::v1::deserialize
