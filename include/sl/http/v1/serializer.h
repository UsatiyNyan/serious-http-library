#pragma once

#include <span>
#include <string_view>

#include "message.hpp"

namespace sl::http::v1 {

class serializer {
public:
    static std::vector<std::byte> serialize(const request_type& req) {
        std::vector<std::byte> result;
        std::byte term {' '};
        result.reserve(req.uri.size() + req.method.size() + req.version.size() + req.body.size());
        auto byte_data = reinterpret_cast<const std::byte*>(req.method.data());
        result.insert(result.end(), byte_data, byte_data + req.method.size());
        result.push_back(term);
        byte_data = reinterpret_cast<const std::byte*>(req.uri.data());
        result.insert(result.end(), byte_data, byte_data + req.uri.size());
        result.push_back(term);
        byte_data = reinterpret_cast<const std::byte*>(req.version.data());
        result.insert(result.end(), byte_data, byte_data + req.version.size());
        result.push_back(std::byte{'\r'});
        result.push_back(std::byte{'\n'});
        prepare_headers(result, req);
        result.push_back(std::byte{'\r'});
        result.push_back(std::byte{'\n'});
        return result;
    }

    static std::vector<std::byte> serialize(const response_type& resp) {
        std::vector<std::byte> result;
        std::byte term {' '};
        auto byte_data = reinterpret_cast<const std::byte*>(resp.version.data());
        result.insert(result.end(), byte_data, byte_data + resp.version.size());
        result.push_back(term);
        byte_data = reinterpret_cast<const std::byte*>(std::to_string(resp.status_code).data());
        result.insert(result.end(), byte_data, byte_data + sizeof(resp.status_code));
        result.push_back(term);
        byte_data = reinterpret_cast<const std::byte*>(resp.reason_phrase.data());
        result.insert(result.end(), byte_data, byte_data + resp.reason_phrase.size());
        result.push_back(std::byte{'\r'});
        result.push_back(std::byte{'\n'});
        prepare_headers(result, resp);
        result.push_back(std::byte{'\r'});
        result.push_back(std::byte{'\n'});
        return result;
    }

private:
    static void prepare_headers(std::vector<std::byte>& result, const message_type& msg) {
        bool has_content_length = msg.has_header("Content-Length");

        if (!msg.body.empty() && !has_content_length) {
            std::string_view cont_length = "Content-Length: ";
            auto byte_data = reinterpret_cast<const std::byte*>(cont_length.data());
            result.insert(result.end(), byte_data, byte_data + cont_length.size());
            std::string cont_value = std::to_string(msg.body.size());
            byte_data = reinterpret_cast<const std::byte*>(cont_value.data());
            result.insert(result.end(), byte_data, byte_data + cont_value.size());
        }

        for (const auto& [header, value] : msg.headers) {
            if (header == "Content-Length") continue;

            auto byte_data = reinterpret_cast<const std::byte*>(header.data());
            result.insert(result.end(), byte_data, byte_data + header.size());

            result.push_back(std::byte{':'});
            result.push_back(std::byte{' '});

            byte_data = reinterpret_cast<const std::byte*>(value.data());
            result.insert(result.end(), byte_data, byte_data + value.size());

            result.push_back(std::byte{'\r'});
            result.push_back(std::byte{'\n'});
        }
    }
};
} // http::v1
