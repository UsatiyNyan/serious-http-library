//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/strings.hpp"
#include "sl/http/v1/serialize/message.hpp"

#include <gtest/gtest.h>

namespace sl::http::v1::serialize {

class SerializeMessageTest : public ::testing::Test {
protected:
    meta::maybe<std::string> serialize(const message_type& msg) {
        std::vector<std::byte> buffer;
        serialize_config config{ .buffer_size = 1024 };
        auto serializer = make_serialize(msg, config);

        std::size_t written = 0;
        while (true) {
            auto span = serializer(written);
            if (span.empty()) {
                break;
            }
            buffer.insert(buffer.end(), span.begin(), span.end());
            written = span.size();
        }
        return std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }
};

// === Request Serialization ===

TEST_F(SerializeMessageTest, RequestOriginForm) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            request_line_type{
                .target =
                    origin_target_type{
                        .path = "/",
                        .query = {},
                    },
                .method = method_type::GET,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value()) << "Serialization failed";
    EXPECT_EQ(result.value(), "GET / HTTP/1.1\r\n\r\n");
}

TEST_F(SerializeMessageTest, RequestOriginFormWithQuery) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            request_line_type{
                .target =
                    origin_target_type{
                        .path = "/search",
                        .query = { { "q", "hello" } },
                    },
                .method = method_type::GET,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "GET /search?q=hello HTTP/1.1\r\n\r\n");
}

TEST_F(SerializeMessageTest, RequestAbsoluteForm) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            request_line_type{
                .target =
                    absolute_target_type{
                        .scheme = "http",
                        .authority = "example.com",
                        .path = "/path",
                        .query = {},
                    },
                .method = method_type::GET,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "GET http://example.com/path HTTP/1.1\r\n\r\n");
}

TEST_F(SerializeMessageTest, RequestAuthorityForm) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            request_line_type{
                .target =
                    authority_target_type{
                        .host = "example.com",
                        .port = 443,
                    },
                .method = method_type::CONNECT,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "CONNECT example.com:443 HTTP/1.1\r\n\r\n");
}

TEST_F(SerializeMessageTest, RequestAsteriskForm) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            request_line_type{
                .target = asterisk_target_type{},
                .method = method_type::OPTIONS,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "OPTIONS * HTTP/1.1\r\n\r\n");
}

TEST_F(SerializeMessageTest, RequestWithHeaders) {
    message_type msg{
        .fields = { { "host", "example.com" }, { "user-agent", "test" } },
        .body = {},
        .start_line =
            request_line_type{
                .target =
                    origin_target_type{
                        .path = "/",
                        .query = {},
                    },
                .method = method_type::GET,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    // Fields order may vary due to map, check contains
    EXPECT_NE(result.value().find("host:example.com\r\n"), std::string::npos);
    EXPECT_NE(result.value().find("user-agent:test\r\n"), std::string::npos);
    EXPECT_TRUE(result.value().starts_with("GET / HTTP/1.1\r\n"));
    EXPECT_TRUE(result.value().ends_with("\r\n\r\n"));
}

TEST_F(SerializeMessageTest, RequestWithBody) {
    auto body_str = std::string("Hello, World!");
    message_type msg{
        .fields = { { "content-length", "13" } },
        .body = std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(body_str.data()),
            reinterpret_cast<const std::byte*>(body_str.data() + body_str.size())
        ),
        .start_line =
            request_line_type{
                .target =
                    origin_target_type{
                        .path = "/submit",
                        .query = {},
                    },
                .method = method_type::POST,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().ends_with("\r\n\r\nHello, World!"));
}

// === Response Serialization ===

TEST_F(SerializeMessageTest, Response200) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            response_line_type{
                .reason = "OK",
                .status = status_type::OK,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "HTTP/1.1 200 OK\r\n\r\n");
}

TEST_F(SerializeMessageTest, Response404) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            response_line_type{
                .reason = "Not Found",
                .status = status_type::NOT_FOUND,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "HTTP/1.1 404 Not Found\r\n\r\n");
}

TEST_F(SerializeMessageTest, ResponseEmptyReason) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            response_line_type{
                .reason = "",
                .status = status_type::OK,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "HTTP/1.1 200 \r\n\r\n");
}

TEST_F(SerializeMessageTest, ResponseWithBody) {
    auto body_str = std::string("<html></html>");
    message_type msg{
        .fields = { { "content-type", "text/html" } },
        .body = std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(body_str.data()),
            reinterpret_cast<const std::byte*>(body_str.data() + body_str.size())
        ),
        .start_line =
            response_line_type{
                .reason = "OK",
                .status = status_type::OK,
                .version = version_type::HTTPv1_1,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().ends_with("\r\n\r\n<html></html>"));
}

TEST_F(SerializeMessageTest, ResponseHTTP10) {
    message_type msg{
        .fields = {},
        .body = {},
        .start_line =
            response_line_type{
                .reason = "OK",
                .status = status_type::OK,
                .version = version_type::HTTPv1_0,
            },
    };
    auto result = serialize(msg);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "HTTP/1.0 200 OK\r\n\r\n");
}

} // namespace sl::http::v1::serialize
