#pragma once

#include <string>
#include <unordered_map>

namespace sl::http::v1 {

struct message_type {
    std::string_view version;
    std::unordered_map<std::string_view, std::string_view> headers;
    std::string_view body;

    [[nodiscard]] bool has_header(const std::string_view name) const {
        return headers.find(name) != headers.end();
    }
};

struct request_type : public message_type{
    std::string_view uri;
    std::string_view method;
};

struct response_type : public message_type {
    unsigned status_code = 200;
    std::string_view reason_phrase = "OK";
};

} // http::v1
