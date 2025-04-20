//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/deserialize_request.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <gtest/gtest.h>

#include <variant>

namespace sl::http::v1::detail::deserialize {
namespace request {

TEST(requestEmpty, toEmpty) {
    const auto result = state_empty{}.process({});
    EXPECT_TRUE(std::holds_alternative<state_empty>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestEmpty, toEmptyShort) {
    const auto result = state_empty{}.process(buffer_str_to_byte("POST"));
    EXPECT_TRUE(std::holds_alternative<state_empty>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestEmpty, toCompleteError) {
    const auto result = state_empty{}.process(buffer_str_to_byte("hehe ..."));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestEmpty, toMethod) {
    const auto result = state_empty{}.process(buffer_str_to_byte("POST ..."));
    const auto* s_m = std::get_if<state_method>(&result.state_variant);
    ASSERT_TRUE(s_m);
    EXPECT_EQ(s_m->method, method_type::POST);
    EXPECT_EQ(result.offset, 5);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestMethod, toMethod) {
    const auto result = state_method{ state_empty{}, method_type::GET }.process(buffer_str_to_byte(" "));
    EXPECT_TRUE(std::holds_alternative<state_method>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestMethod, toCompleteError) {
    const auto result = state_method{ state_empty{}, method_type::GET }.process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestMethod, toTarget) {
    const auto result = state_method{ state_empty{}, method_type::GET }.process(buffer_str_to_byte("/path "));
    const auto* s_t = std::get_if<state_target>(&result.state_variant);
    ASSERT_TRUE(s_t);
    EXPECT_EQ(s_t->target, "/path");
    EXPECT_EQ(result.offset, 6);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestTarget, toTarget) {
    const auto result =
        state_target{ state_method{ state_empty{}, method_type::GET }, "/path" }.process(buffer_str_to_byte(" "));
    EXPECT_TRUE(std::holds_alternative<state_target>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestTarget, toCompleteError) {
    const auto result =
        state_target{ state_method{ state_empty{}, method_type::GET }, "/path" }.process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestTarget, toVersion) {
    const auto result =
        state_target{
            state_method{ state_empty{}, method_type::GET },
            "/path",
        }
            .process(buffer_str_to_byte("HTTP/1.1 "));
    const auto* s_v = std::get_if<state_version>(&result.state_variant);
    ASSERT_TRUE(s_v);
    EXPECT_EQ(s_v->version, version_type::HTTPv1_1);
    EXPECT_EQ(result.offset, 8);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestVersion, toVersion) {
    const auto result =
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
            .process(buffer_str_to_byte("\r\n"));
    EXPECT_TRUE(std::holds_alternative<state_version>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestVersion, toCompleteError) {
    const auto result =
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
            .process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestVersion, toFields) {
    const auto result =
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
            .process(buffer_str_to_byte("\r\n"));
    const auto* s_f = std::get_if<state_fields>(&result.state_variant);
    ASSERT_TRUE(s_f);
    EXPECT_EQ(result.offset, 2);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestFields, toFieldsEmpty) {
    const auto result = state_fields{
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
    }.process(buffer_str_to_byte("\r\n"));
    EXPECT_TRUE(std::holds_alternative<state_fields>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestFields, toFieldsPartial) {
    const auto result = state_fields{
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
    }.process(buffer_str_to_byte("Host: example.com\r\n"));
    EXPECT_TRUE(std::holds_alternative<state_fields>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestFields, toFieldsFull) {
    const auto result = state_fields{
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
    }.process(buffer_str_to_byte("Host: example.com\r\n\r\n"));
    const auto* s_b = std::get_if<state_body>(&result.state_variant);
    ASSERT_TRUE(s_b);
    EXPECT_EQ(result.offset, 19);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestFields, toCompleteError) {
    const auto result = state_fields{
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
    }.process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestFields, toBody) {
    const auto result = state_fields{
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
    }.process(buffer_str_to_byte("Content-Length: 5\r\n\r\n"));
    const auto* s_b = std::get_if<state_body>(&result.state_variant);
    ASSERT_TRUE(s_b);
    EXPECT_EQ(s_b->content_length, 5);
    EXPECT_EQ(result.offset, 21);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestFields, toChunkedBody) {
    const auto result = state_fields{
        state_version{
            state_target{
                state_method{ state_empty{}, method_type::GET },
                "/path",
            },
            version_type::HTTPv1_1,
        }
    }.process(buffer_str_to_byte("Transfer-Encoding: chunked\r\n\r\n"));
    const auto* s_cb = std::get_if<state_chunked_body>(&result.state_variant);
    ASSERT_TRUE(s_cb);
    EXPECT_EQ(result.offset, 30);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestBody, toBodyEmpty) {
    const auto result =
        state_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
            5,
        }
            .process(buffer_str_to_byte(""));
    EXPECT_TRUE(std::holds_alternative<state_body>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestBody, toBodyPartial) {
    const auto result =
        state_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
            5,
        }
            .process(buffer_str_to_byte("123"));
    EXPECT_TRUE(std::holds_alternative<state_body>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestBody, toCompleteError) {
    const auto result =
        state_body{
            state_fields{ state_version{
                state_target{
                    state_method{ state_empty{}, method_type::GET },
                    "/path",
                },
                version_type::HTTPv1_1,
            } },
            5,
        }
            .process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestBody, toCompleteOk) {
    const auto result =
        state_body{
            state_fields{ state_version{
                state_target{
                    state_method{ state_empty{}, method_type::GET },
                    "/path",
                },
                version_type::HTTPv1_1,
            } },
            5,
        }
            .process(buffer_str_to_byte("12345"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_TRUE(s_c->result.has_value());
    EXPECT_EQ(result.offset, 5);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestChunkedBody, toChunkedBody) {
    // TEST: toChunkedBody... with particular cases, when implemented
}

TEST(requestChunkedBody, toCompleteError) {
    const auto result = state_chunked_body{
        state_fields{
            state_version{
                state_target{
                    state_method{ state_empty{}, method_type::GET },
                    "/path",
                },
                version_type::HTTPv1_1,
            },
        }
    }.process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestChunkedBody, toTrailingFields) {
    const auto result = state_chunked_body{
        state_fields{
            state_version{
                state_target{
                    state_method{ state_empty{}, method_type::GET },
                    "/path",
                },
                version_type::HTTPv1_1,
            },
        }
    }.process(buffer_str_to_byte("0\r\n\r\n"));
    const auto* s_tf = std::get_if<state_trailing_fields>(&result.state_variant);
    ASSERT_TRUE(s_tf);
    EXPECT_EQ(result.offset, 5);
    EXPECT_TRUE(result.can_continue);
}

TEST(requestTrailingFields, toTrailingFieldsEmpty) {
    const auto result = state_trailing_fields{
        state_chunked_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
        }
    }.process(buffer_str_to_byte("\r\n"));
    EXPECT_TRUE(std::holds_alternative<state_trailing_fields>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestTrailingFields, toTrailingFieldsPartial) {
    const auto result = state_trailing_fields{
        state_chunked_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
        }
    }.process(buffer_str_to_byte("Trailer: value\r\n"));
    EXPECT_TRUE(std::holds_alternative<state_trailing_fields>(result.state_variant));
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestTrailingFields, toTrailingFieldsFull) {
    const auto result = state_trailing_fields{
        state_chunked_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
        }
    }.process(buffer_str_to_byte("Trailer: value\r\n\r\n"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_TRUE(s_c->result.has_value());
    EXPECT_EQ(result.offset, 17);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestTrailingFields, toCompleteError) {
    const auto result = state_trailing_fields{
        state_chunked_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
        }
    }.process(buffer_str_to_byte("INVALID"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_FALSE(s_c->result.has_value());
    EXPECT_EQ(s_c->result.error(), status_type::BAD_REQUEST);
    EXPECT_EQ(result.offset, 0);
    EXPECT_FALSE(result.can_continue);
}

TEST(requestTrailingFields, toCompleteOk) {
    const auto result = state_trailing_fields{
        state_chunked_body{
            state_fields{
                state_version{
                    state_target{
                        state_method{ state_empty{}, method_type::GET },
                        "/path",
                    },
                    version_type::HTTPv1_1,
                },
            },
        }
    }.process(buffer_str_to_byte("\r\n"));
    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    ASSERT_TRUE(s_c->result.has_value());
    EXPECT_EQ(result.offset, 2);
    EXPECT_FALSE(result.can_continue);
}

} // namespace request
} // namespace sl::http::v1::detail::deserialize
