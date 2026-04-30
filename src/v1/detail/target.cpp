//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/target.hpp"
#include "sl/http/v1/detail/percent_encoding.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <charconv>

namespace sl::http::v1::deserialize {
namespace detail {

meta::maybe<query_params> parse_query_string(std::string_view query_str) {
    query_params result;

    while (!query_str.empty()) {
        // split by '&'
        auto pair_split = try_find_split_unlimited(query_str, "&");
        std::string_view pair_str = pair_split.head;

        if (!pair_str.empty()) {
            // split by '=' for key-value
            auto kv_split = try_find_split_unlimited(pair_str, "=");

            auto maybe_key = percent_decode_query(kv_split.head);
            if (!maybe_key.has_value()) {
                return meta::null;
            }

            std::string value;
            if (kv_split.tail.has_value()) {
                auto maybe_value = percent_decode_query(kv_split.tail.value());
                if (!maybe_value.has_value()) {
                    return meta::null;
                }
                value = std::move(maybe_value).value();
            }

            result.emplace_back(std::move(maybe_key).value(), std::move(value));
        }

        if (!pair_split.tail.has_value()) {
            break;
        }
        query_str = pair_split.tail.value();
    }

    return result;
}

meta::maybe<origin_target_type> parse_origin_form(std::string_view target_str) {
    // must start with '/'
    if (!target_str.starts_with('/')) {
        return meta::null;
    }

    // split at '?' for query
    auto split = try_find_split_unlimited(target_str, "?");

    std::string_view raw_path = split.head;
    std::string_view raw_query = split.tail.value_or(std::string_view{});

    // decode path
    auto maybe_path = percent_decode(raw_path);
    if (!maybe_path.has_value()) {
        return meta::null;
    }

    // parse query if present
    query_params query;
    if (!raw_query.empty()) {
        auto maybe_query = parse_query_string(raw_query);
        if (!maybe_query.has_value()) {
            return meta::null;
        }
        query = std::move(maybe_query).value();
    }

    return origin_target_type{
        .path = std::move(maybe_path).value(),
        .raw_path = std::string{ raw_path },
        .query = std::move(query),
        .raw_query = std::string{ raw_query },
    };
}

meta::maybe<absolute_target_type> parse_absolute_form(std::string_view target_str) {
    // extract scheme
    std::string scheme;
    if (target_str.starts_with("https://")) {
        scheme = "https";
        target_str.remove_prefix(8);
    } else if (target_str.starts_with("http://")) {
        scheme = "http";
        target_str.remove_prefix(7);
    } else {
        return meta::null;
    }

    // find authority/path boundary (first '/' after scheme)
    auto path_split = try_find_split_unlimited(target_str, "/");
    std::string_view authority = path_split.head;

    if (authority.empty()) {
        return meta::null;
    }

    // path + query (prepend '/' since split consumed it)
    std::string raw_path = "/";
    std::string_view raw_query;

    if (path_split.tail.has_value()) {
        std::string_view path_and_query = path_split.tail.value();
        auto query_split = try_find_split_unlimited(path_and_query, "?");
        raw_path += query_split.head;
        raw_query = query_split.tail.value_or(std::string_view{});
    }

    auto maybe_path = percent_decode(raw_path);
    if (!maybe_path.has_value()) {
        return meta::null;
    }

    query_params query;
    if (!raw_query.empty()) {
        auto maybe_query = parse_query_string(raw_query);
        if (!maybe_query.has_value()) {
            return meta::null;
        }
        query = std::move(maybe_query).value();
    }

    return absolute_target_type{
        .scheme = std::move(scheme),
        .authority = std::string{ authority },
        .path = std::move(maybe_path).value(),
        .raw_path = std::move(raw_path),
        .query = std::move(query),
        .raw_query = std::string{ raw_query },
    };
}

meta::maybe<authority_target_type> parse_authority_form(std::string_view target_str) {
    // find last ':' for host:port split
    auto pos = target_str.rfind(':');
    if (pos == std::string_view::npos) {
        return meta::null;
    }

    std::string_view host = target_str.substr(0, pos);
    std::string_view port_str = target_str.substr(pos + 1);

    // host cannot be empty
    if (host.empty()) {
        return meta::null;
    }

    // port must be numeric and in range
    if (port_str.empty()) {
        return meta::null;
    }

    std::uint16_t port = 0;
    auto conv_result = std::from_chars(port_str.data(), port_str.data() + port_str.size(), port);
    if (conv_result.ec != std::errc{} || conv_result.ptr != port_str.data() + port_str.size()) {
        return meta::null;
    }

    return authority_target_type{
        .host = std::string{ host },
        .port = port,
    };
}

} // namespace detail

meta::maybe<target_type> target(std::string_view target_str) {
    if (target_str.empty()) {
        return meta::null;
    }

    // asterisk-form: exactly "*"
    if (target_str == "*") {
        return asterisk_target_type{};
    }

    // origin-form: starts with "/"
    if (target_str.starts_with('/')) {
        return detail::parse_origin_form(target_str).map([](auto&& form) -> target_type { return std::move(form); });
    }

    // absolute-form: starts with scheme
    if (target_str.starts_with("http://") || target_str.starts_with("https://")) {
        return detail::parse_absolute_form(target_str).map([](auto&& form) -> target_type { return std::move(form); });
    }

    // authority-form: host:port (contains ':' but no '/')
    if (target_str.find(':') != std::string_view::npos && target_str.find('/') == std::string_view::npos) {
        return detail::parse_authority_form(target_str).map([](auto&& form) -> target_type { return std::move(form); });
    }

    return meta::null;
}

} // namespace sl::http::v1::deserialize
