#include <gtest/gtest.h>

#include "sl/http/v1/parser.h"

namespace sl::http::v1 {

TEST(func, get1) {
    std::string_view get_req = "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "Accept: text/html\r\n"
        "\r\n"
        "TESTBODY";

    auto parse_result = parse({reinterpret_cast<const std::byte*>(get_req.data()), get_req.size()});
    auto value = parse_result.value();
    EXPECT_EQ(value.method, "GET");
    EXPECT_EQ(value.uri, "/index.html");
    EXPECT_EQ(value.version, "HTTP/1.1");
    EXPECT_EQ(value.body, "TESTBODY");
    EXPECT_EQ(value.headers.size(), 3);
}

}