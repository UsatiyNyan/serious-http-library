//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/strings.hpp"

#include <gtest/gtest.h>

namespace sl::http::v1::deserialize::detail {

enum class test_enum {
    CCC,
    A,
    DDDD,
    BB,
    EEEEE,
    ENUM_END,
};

constexpr std::string_view enum_to_str(test_enum e) {
    switch (e) {
    case test_enum::CCC:
        return "CCC";
    case test_enum::A:
        return "A";
    case test_enum::DDDD:
        return "DDDD";
    case test_enum::BB:
        return "BB";
    case test_enum::EEEEE:
        return "EEEEE";
    default:
        break;
    }
    return {};
}

TEST(v1DetailStrings, enumMaxStrLength) {
    constexpr auto test_enum_max_str_length = enum_max_str_length<test_enum>();
    static_assert(test_enum_max_str_length == 5);
}

TEST(v1DetailStrings, stripSuffix) {
    EXPECT_EQ(strip_suffix("abcd", "d"), "abc");
    EXPECT_EQ(strip_suffix("abcd", "c"), "abcd");
    EXPECT_EQ(strip_suffix(" abcd", " "), " abcd");
    EXPECT_EQ(strip_suffix("  abcd  ", " "), "  abcd ");
    EXPECT_EQ(strip_suffix("  abcd  ", "  "), "  abcd");
    EXPECT_EQ(strip_suffix("\tabcd\t", "\t"), "\tabcd");
}

TEST(v1DetailStrings, stripPrefix) {
    EXPECT_EQ(strip_prefix("abcd", "a"), "bcd");
    EXPECT_EQ(strip_prefix("abcd", "b"), "abcd");
    EXPECT_EQ(strip_prefix("abcd ", " "), "abcd ");
    EXPECT_EQ(strip_prefix("  abcd  ", " "), " abcd  ");
    EXPECT_EQ(strip_prefix("  abcd  ", "  "), "abcd  ");
    EXPECT_EQ(strip_prefix("\tabcd\t", "\t"), "abcd\t");
}

TEST(v1DetailStrings, stripSuffixWhile) {
    EXPECT_EQ(strip_suffix_while("abcd", tokens::is_ws), "abcd");
    EXPECT_EQ(strip_suffix_while("ab cd", tokens::is_ws), "ab cd");
    EXPECT_EQ(strip_suffix_while(" abcd", tokens::is_ws), " abcd");
    EXPECT_EQ(strip_suffix_while("  abcd  ", tokens::is_ws), "  abcd");
    EXPECT_EQ(strip_suffix_while("\tabcd\t \t\t\t", tokens::is_ws), "\tabcd");
}

TEST(v1DetailStrings, stripPrefixWhile) {
    EXPECT_EQ(strip_prefix_while("abcd", tokens::is_ws), "abcd");
    EXPECT_EQ(strip_prefix_while("ab\tcd", tokens::is_ws), "ab\tcd");
    EXPECT_EQ(strip_prefix_while("abcd \t", tokens::is_ws), "abcd \t");
    EXPECT_EQ(strip_prefix_while("  abcd  ", tokens::is_ws), "abcd  ");
    EXPECT_EQ(strip_prefix_while("\t\t\t\t  \tabcd\t", tokens::is_ws), "abcd\t");
}
TEST(v1DetailStrings, tryFindUnlimited) {
    {
        const std::string_view haystack = "hayhayhayhayneedlehayhayhay";
        const auto result = try_find_unlimited(haystack, "needle");
        ASSERT_TRUE(result.has_value());
        const auto [prefix, offset] = result.value();
        EXPECT_EQ(prefix, "hayhayhayhay");
        EXPECT_EQ(offset, 18);
        EXPECT_EQ(haystack.substr(offset), "hayhayhay");
    }

    {
        const std::string_view haystack = "hayhayhayhayneedlehayhayhay";
        const auto result = try_find_unlimited(haystack, "somethingelse");
        ASSERT_FALSE(result.has_value());
        const auto error = result.error();
        EXPECT_EQ(error, find_err::NOT_FOUND);
    }

    {
        const std::string_view haystack = "hay";
        const auto result = try_find_unlimited(haystack, "needle");
        ASSERT_FALSE(result.has_value());
        const auto error = result.error();
        EXPECT_EQ(error, find_err::NOT_FOUND);
    }

    {
        const std::string_view haystack = "";
        const auto result = try_find_unlimited(haystack, "needle");
        ASSERT_FALSE(result.has_value());
        const auto error = result.error();
        EXPECT_EQ(error, find_err::NOT_FOUND);
    }
}

TEST(v1DetailStrings, tryFindLimited) {
    {
        const std::string_view haystack = "hayhayhayhayneedlehayhayhay";
        const std::string_view needle = "needle";
        const auto result = try_find(haystack, needle, haystack.size() - needle.size());
        ASSERT_TRUE(result.has_value());
        const auto [prefix, offset] = result.value();
        EXPECT_EQ(prefix, "hayhayhayhay");
        EXPECT_EQ(offset, 18);
        EXPECT_EQ(haystack.substr(offset), "hayhayhay");
    }

    {
        const std::string_view haystack = "hayhayhayhayhayhayhayneedle";
        const std::string_view needle = "needle";
        const auto result = try_find(haystack, needle, haystack.size() - needle.size());
        ASSERT_TRUE(result.has_value());
        const auto [prefix, offset] = result.value();
        EXPECT_EQ(prefix, "hayhayhayhayhayhayhay");
        EXPECT_EQ(offset, haystack.size());
        EXPECT_EQ(haystack.substr(offset), "");
    }

    {
        const std::string_view haystack = "hayhayhayhayhayhayhayneedle";
        const std::string_view needle = "needle";
        const auto result = try_find(haystack, needle, haystack.size() - needle.size() - 1);
        ASSERT_FALSE(result.has_value());
        const auto error = result.error();
        EXPECT_EQ(error, find_err::MAX_SIZE_EXCEEDED);
    }

    {
        const std::string_view haystack = "hayhayhayhayneedlehayhayhay";
        const std::string_view needle = "needle";
        const auto result = try_find(haystack, needle, haystack.max_size() - needle.size());
        ASSERT_TRUE(result.has_value());
        const auto [prefix, offset] = result.value();
        EXPECT_EQ(prefix, "hayhayhayhay");
        EXPECT_EQ(offset, 18);
        EXPECT_EQ(haystack.substr(offset), "hayhayhay");
    }

#if NDEBUG // otherwise this test fails
    {
        const std::string_view haystack = "hayhayhayhayneedlehayhayhay";
        const std::string_view needle = "needle";
        const auto result = try_find(haystack, needle, haystack.max_size() - needle.size() + 1);
        ASSERT_FALSE(result.has_value());
        const auto error = result.error();
        EXPECT_EQ(error, find_err::MAX_SIZE_EXCEEDED);
    }
#endif

    {
        const std::string_view haystack = "hayhayhayhayneedlehayhayhay";
        const auto result = try_find(haystack, "somethingelse", haystack.size());
        ASSERT_FALSE(result.has_value());
        const auto error = result.error();
        EXPECT_EQ(error, find_err::NOT_FOUND);
    }
}

TEST(v1DetailStrings, tryFindSplit) {
    {
        const auto result = try_find_split_unlimited("", ", ");
        ASSERT_FALSE(result.has_value());
    }

    {
        const auto result = try_find_split_unlimited("nonempty", ", ");
        ASSERT_TRUE(result.has_value());
        const auto [found, remainder] = result.value();
        EXPECT_EQ(found, "nonempty");
    }

    const std::string_view series_str = "aaa, bbb, ccc, dddd";

    {
        const auto result = try_find_split_unlimited(series_str, ",");
        ASSERT_TRUE(result.has_value());
        const auto [found, remainder] = result.value();
        EXPECT_EQ(found, "aaa");
        EXPECT_EQ(remainder, " bbb, ccc, dddd");
    }

    {
        const auto result = try_find_split_unlimited(series_str, ", ");
        ASSERT_TRUE(result.has_value());
        const auto [found, remainder] = result.value();
        EXPECT_EQ(found, "aaa");
        EXPECT_EQ(remainder, "bbb, ccc, dddd");
    }

    {
        std::string_view series_state = series_str;
        std::vector<std::string_view> series_values;
        while (const auto maybe_next = try_find_split_unlimited(series_state, ", ")) {
            const auto [next, remainder] = maybe_next.value();
            series_values.push_back(next);
            series_state = remainder;
        }
        const std::vector<std::string_view> expected_series_values{
            "aaa",
            "bbb",
            "ccc",
            "dddd",
        };
        EXPECT_EQ(series_values, expected_series_values);
    }
}

} // namespace sl::http::v1::deserialize::detail
