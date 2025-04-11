//
// Created by usatiynyan.
//

#include "sl/http/v1/deserialize.hpp"

#include <gtest/gtest.h>

namespace sl::http::v1 {

TEST(deserialize, invalidRequest) {
    const auto request_str{ R"(eheheh\r\n)" };
    const auto request_result = deserialize_request(request_str);
    ASSERT_FALSE(request_result.has_value());
}

TEST(deserialize, invalidResponse) {
    const auto response_str{ R"(eheheh\r\n)" };
    const auto response_result = deserialize_response(response_str);
    ASSERT_FALSE(response_result.has_value());
}

TEST(deserialize, simpleRequest) {
    const auto request_str{ 
        R"(eheheh\r\n)" 
    };
    const auto request_result = deserialize_request(request_str);
    ASSERT_FALSE(request_result.has_value());
}

TEST(deserialize, simpleResponse) {
    const auto response_str{ 
        R"(eheheh\r\n)" 
    };
    const auto response_result = deserialize_response(response_str);
    ASSERT_FALSE(response_result.has_value());
}

} // namespace sl::http::v1
