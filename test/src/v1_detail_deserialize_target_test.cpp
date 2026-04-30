//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/target.hpp"

#include <gtest/gtest.h>

namespace sl::http::v1::deserialize {

// === Asterisk Form ===

TEST(DeserializeTarget, AsteriskForm) {
    auto result = target("*");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<asterisk_target_type>(result.value()));
}

// === Origin Form ===

TEST(DeserializeTarget, OriginFormRoot) {
    auto result = target("/");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    EXPECT_EQ(origin->path, "/");
    EXPECT_EQ(origin->raw_path, "/");
    EXPECT_TRUE(origin->query.empty());
    EXPECT_TRUE(origin->raw_query.empty());
}

TEST(DeserializeTarget, OriginFormWithPath) {
    auto result = target("/api/users/123");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    EXPECT_EQ(origin->path, "/api/users/123");
    EXPECT_EQ(origin->raw_path, "/api/users/123");
    EXPECT_TRUE(origin->query.empty());
}

TEST(DeserializeTarget, OriginFormWithQuery) {
    auto result = target("/search?q=hello&page=1");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    EXPECT_EQ(origin->path, "/search");
    EXPECT_EQ(origin->raw_path, "/search");
    EXPECT_EQ(origin->raw_query, "q=hello&page=1");
    ASSERT_EQ(origin->query.size(), 2);
    EXPECT_EQ(origin->query[0], std::make_pair(std::string{"q"}, std::string{"hello"}));
    EXPECT_EQ(origin->query[1], std::make_pair(std::string{"page"}, std::string{"1"}));
}

TEST(DeserializeTarget, OriginFormPercentDecoded) {
    auto result = target("/path%20with%20spaces?name=John%20Doe");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    EXPECT_EQ(origin->path, "/path with spaces");
    EXPECT_EQ(origin->raw_path, "/path%20with%20spaces");
    ASSERT_EQ(origin->query.size(), 1);
    EXPECT_EQ(origin->query[0].first, "name");
    EXPECT_EQ(origin->query[0].second, "John Doe");
}

TEST(DeserializeTarget, OriginFormQueryPlusAsSpace) {
    auto result = target("/search?q=hello+world");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    ASSERT_EQ(origin->query.size(), 1);
    EXPECT_EQ(origin->query[0].second, "hello world");
}

TEST(DeserializeTarget, OriginFormEmptyQueryValue) {
    auto result = target("/path?flag&key=");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    ASSERT_EQ(origin->query.size(), 2);
    EXPECT_EQ(origin->query[0], std::make_pair(std::string{"flag"}, std::string{}));
    EXPECT_EQ(origin->query[1], std::make_pair(std::string{"key"}, std::string{}));
}

TEST(DeserializeTarget, OriginFormDuplicateKeys) {
    auto result = target("/path?key=1&key=2&key=3");
    ASSERT_TRUE(result.has_value());
    auto* origin = std::get_if<origin_target_type>(&result.value());
    ASSERT_NE(origin, nullptr);
    ASSERT_EQ(origin->query.size(), 3);
    EXPECT_EQ(origin->query[0].first, "key");
    EXPECT_EQ(origin->query[0].second, "1");
    EXPECT_EQ(origin->query[1].first, "key");
    EXPECT_EQ(origin->query[1].second, "2");
    EXPECT_EQ(origin->query[2].first, "key");
    EXPECT_EQ(origin->query[2].second, "3");
}

// === Absolute Form ===

TEST(DeserializeTarget, AbsoluteFormHttp) {
    auto result = target("http://example.com/path?q=1");
    ASSERT_TRUE(result.has_value());
    auto* absolute = std::get_if<absolute_target_type>(&result.value());
    ASSERT_NE(absolute, nullptr);
    EXPECT_EQ(absolute->scheme, "http");
    EXPECT_EQ(absolute->authority, "example.com");
    EXPECT_EQ(absolute->path, "/path");
    EXPECT_EQ(absolute->raw_path, "/path");
    ASSERT_EQ(absolute->query.size(), 1);
    EXPECT_EQ(absolute->query[0].first, "q");
}

TEST(DeserializeTarget, AbsoluteFormHttps) {
    auto result = target("https://secure.example.com:8443/api");
    ASSERT_TRUE(result.has_value());
    auto* absolute = std::get_if<absolute_target_type>(&result.value());
    ASSERT_NE(absolute, nullptr);
    EXPECT_EQ(absolute->scheme, "https");
    EXPECT_EQ(absolute->authority, "secure.example.com:8443");
    EXPECT_EQ(absolute->path, "/api");
}

TEST(DeserializeTarget, AbsoluteFormRootOnly) {
    auto result = target("http://example.com/");
    ASSERT_TRUE(result.has_value());
    auto* absolute = std::get_if<absolute_target_type>(&result.value());
    ASSERT_NE(absolute, nullptr);
    EXPECT_EQ(absolute->path, "/");
}

TEST(DeserializeTarget, AbsoluteFormNoPath) {
    auto result = target("http://example.com");
    ASSERT_TRUE(result.has_value());
    auto* absolute = std::get_if<absolute_target_type>(&result.value());
    ASSERT_NE(absolute, nullptr);
    EXPECT_EQ(absolute->authority, "example.com");
    EXPECT_EQ(absolute->path, "/");
}

// === Authority Form ===

TEST(DeserializeTarget, AuthorityForm) {
    auto result = target("example.com:443");
    ASSERT_TRUE(result.has_value());
    auto* authority = std::get_if<authority_target_type>(&result.value());
    ASSERT_NE(authority, nullptr);
    EXPECT_EQ(authority->host, "example.com");
    EXPECT_EQ(authority->port, 443);
}

TEST(DeserializeTarget, AuthorityFormLocalhost) {
    auto result = target("localhost:8080");
    ASSERT_TRUE(result.has_value());
    auto* authority = std::get_if<authority_target_type>(&result.value());
    ASSERT_NE(authority, nullptr);
    EXPECT_EQ(authority->host, "localhost");
    EXPECT_EQ(authority->port, 8080);
}

TEST(DeserializeTarget, AuthorityFormMaxPort) {
    auto result = target("host:65535");
    ASSERT_TRUE(result.has_value());
    auto* authority = std::get_if<authority_target_type>(&result.value());
    ASSERT_NE(authority, nullptr);
    EXPECT_EQ(authority->port, 65535);
}

// === Error Cases ===

TEST(DeserializeTarget, EmptyTarget) {
    auto result = target("");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidPercentEncodingBadHex) {
    auto result = target("/path%GG");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidPercentEncodingIncomplete) {
    auto result = target("/path%2");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidPercentEncodingTrailing) {
    auto result = target("/path%");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidPortOverflow) {
    auto result = target("host:99999");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidPortNonNumeric) {
    auto result = target("host:abc");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidPortEmptyHost) {
    auto result = target(":443");
    EXPECT_FALSE(result.has_value());
}

TEST(DeserializeTarget, InvalidForm) {
    auto result = target("not-a-valid-target");
    EXPECT_FALSE(result.has_value());
}

} // namespace sl::http::v1::deserialize
