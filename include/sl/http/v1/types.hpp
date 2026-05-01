//
// Created by usatiynyan.
// Data modelled according to RFC 9112.
//

#pragma once

#include "sl/http/v1/types/fields.hpp"
#include "sl/http/v1/types/method.hpp"
#include "sl/http/v1/types/status.hpp"
#include "sl/http/v1/types/target.hpp"
#include "sl/http/v1/types/version.hpp"

#include <vector>

namespace sl::http::v1 {

using body_type = std::vector<std::byte>;
using reason_type = std::string_view;

struct request_line_type {
    target_type target;
    method_type method;
    version_type version;
};
struct status_line_type {
    reason_type reason; // can be empty
    status_type status;
    version_type version;
};
using start_line_type = std::variant<request_line_type, status_line_type>;

struct message_type {
    fields_type fields; // can be empty
    body_type body; // can be empty
    start_line_type start_line;
};

struct response_message {
    fields_type fields; // can be empty
    body_type body; // can be empty
    reason_type reason; // can be empty
    status_type status;
    version_type version;
};

} // namespace sl::http::v1
