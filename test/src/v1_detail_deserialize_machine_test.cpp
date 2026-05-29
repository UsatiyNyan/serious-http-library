//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/machine.hpp"

#include <gtest/gtest.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>

#include <cstring>
#include <random>

namespace sl::http::v1::detail {

inline bool span_eq(std::span<const std::byte> a, std::span<const std::byte> b) {
    if (a.size() != b.size()) {
        return false;
    }
    if (a.empty()) {
        return true;
    }
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

struct logging_allocator_state {
    std::size_t alloc_counter = 0;
    std::size_t dealloc_counter = 0;
};

template <typename T>
struct logging_allocator : std::allocator<T> {
    logging_allocator(logging_allocator_state& state) : state_{ &state } {}

    constexpr T* allocate(std::size_t n) {
        T* result = std::allocator<T>::allocate(n);
        if (result != nullptr) {
            state_->alloc_counter += n;
        }
        return result;
    }

    constexpr void deallocate(T* p, std::size_t n) {
        std::allocator<T>::deallocate(p, n);
        state_->dealloc_counter += n;
    }

public:
    logging_allocator_state* state_;
};

struct RemainderBuffer : ::testing::Test {
    void SetUp() override {
        random_engine.seed(1234);
        allocator_state = {};
    }
    void TearDown() override {}

protected:
    template <std::size_t N>
    std::array<std::byte, N> MakeInput() const {
        std::array<std::byte, N> input;
        for (std::byte& byte : input) {
            byte = static_cast<std::byte>(random_engine());
        }
        return input;
    }

protected:
    static std::mt19937 random_engine;
    logging_allocator_state allocator_state{};
    logging_allocator<std::byte> allocator{ allocator_state };
};

std::mt19937 RemainderBuffer::random_engine{};

TEST_F(RemainderBuffer, ctr) {
    remainder_buffer<> rb;
    ASSERT_TRUE(span_eq(rb.view(), std::span<const std::byte>{}));
}

TEST_F(RemainderBuffer, merge) {
    remainder_buffer rb{ 0, allocator };

    const auto input = MakeInput<1024>();
    const auto prev_offset = rb.merge(input);

    EXPECT_EQ(prev_offset, 0);
    EXPECT_TRUE(span_eq(rb.view(), input));
    EXPECT_EQ(allocator_state.alloc_counter, 1024);
}

TEST_F(RemainderBuffer, addOffset) {
    remainder_buffer rb;

#if !defined(NDEBUG)
    EXPECT_DEATH({ rb.add_offset(42); }, ".*");
#endif
}

TEST_F(RemainderBuffer, mergeAddOffset) {
    remainder_buffer rb{ 0, allocator };

    const auto input = MakeInput<1024>();
    std::ignore = rb.merge(input);
    rb.add_offset(512);
    EXPECT_TRUE(span_eq(rb.view(), std::span(input).subspan(512)));
    rb.add_offset(256);
    EXPECT_TRUE(span_eq(rb.view(), std::span(input).subspan(512 + 256)));
    rb.add_offset(256);
    EXPECT_TRUE(rb.view().empty());
    rb.add_offset(0);
    EXPECT_TRUE(rb.view().empty());
    EXPECT_EQ(allocator_state.alloc_counter, 1024);

#if !defined(NDEBUG)
    EXPECT_DEATH({ rb.add_offset(1); }, ".*");
#endif
}

TEST_F(RemainderBuffer, mergeAddOffsetMerge) {
    remainder_buffer rb{ 0, allocator };

    const auto input = MakeInput<2048>();
    const auto input_first = std::span(input).subspan(0, 1024);
    const auto input_second = std::span(input).subspan(1024);
    std::ignore = rb.merge(input_first);
    rb.add_offset(512);
    EXPECT_EQ(rb.merge(input_second), 512);
    EXPECT_TRUE(span_eq(rb.view(), std::span(input).subspan(512)));
    EXPECT_GE(allocator_state.alloc_counter, 512 + 1024);
    EXPECT_LE(allocator_state.alloc_counter, (512 + 1024) * 2);

    rb.add_offset(1024);
    EXPECT_TRUE(span_eq(rb.view(), std::span(input).subspan(512 + 1024)));
    EXPECT_LE(allocator_state.alloc_counter, (512 + 1024) * 2);
}

// Verify: fully consumed buffer doesn't accumulate on subsequent merges
TEST_F(RemainderBuffer, mergeFullyConsumedNoGrowth) {
    remainder_buffer rb{ 0, allocator };

    for (int i = 0; i < 100; ++i) {
        const auto input = MakeInput<256>();
        std::ignore = rb.merge(input);
        EXPECT_TRUE(span_eq(rb.view(), input));
        rb.add_offset(256); // fully consume
        EXPECT_TRUE(rb.view().empty());
    }

    // Buffer replaced each time, not accumulated
    // alloc_counter tracks total allocations, not current size
    // Key: view is empty after each consume, so merge assigns (not appends)
}

// Verify: unconsumed data accumulates (documents unbounded growth potential)
TEST_F(RemainderBuffer, mergeUnconsumedGrows) {
    remainder_buffer rb{ 0, allocator };

    const auto chunk1 = MakeInput<256>();
    const auto chunk2 = MakeInput<256>();
    const auto chunk3 = MakeInput<256>();

    std::ignore = rb.merge(chunk1);
    EXPECT_EQ(rb.view().size(), 256);

    // No consumption - merge appends
    std::ignore = rb.merge(chunk2);
    EXPECT_EQ(rb.view().size(), 512);

    std::ignore = rb.merge(chunk3);
    EXPECT_EQ(rb.view().size(), 768);

    // Data preserved in order
    EXPECT_TRUE(span_eq(rb.view().subspan(0, 256), chunk1));
    EXPECT_TRUE(span_eq(rb.view().subspan(256, 256), chunk2));
    EXPECT_TRUE(span_eq(rb.view().subspan(512, 256), chunk3));
}

// Verify: partial consumption + merge keeps unconsumed prefix
TEST_F(RemainderBuffer, mergePartialConsumeKeepsRemainder) {
    remainder_buffer rb{ 0, allocator };

    const auto chunk1 = MakeInput<256>();
    const auto chunk2 = MakeInput<256>();

    std::ignore = rb.merge(chunk1);
    rb.add_offset(100); // consume 100, leave 156

    const auto prev_offset = rb.merge(chunk2);
    EXPECT_EQ(prev_offset, 100);
    EXPECT_EQ(rb.view().size(), 156 + 256); // unconsumed + new

    // First part is remainder from chunk1
    EXPECT_TRUE(span_eq(rb.view().subspan(0, 156), std::span(chunk1).subspan(100)));
    // Second part is chunk2
    EXPECT_TRUE(span_eq(rb.view().subspan(156, 256), chunk2));
}

// Verify: repeated partial consumption pattern (simulates slow parsing)
TEST_F(RemainderBuffer, repeatedPartialConsumption) {
    remainder_buffer rb{ 0, allocator };

    // Simulate: receive 100 bytes, parse 50, receive 100, parse 50, ...
    std::size_t total_received = 0;
    std::size_t total_consumed = 0;

    for (int i = 0; i < 10; ++i) {
        const auto chunk = MakeInput<100>();
        std::ignore = rb.merge(chunk);
        total_received += 100;

        rb.add_offset(50);
        total_consumed += 50;

        EXPECT_EQ(rb.view().size(), total_received - total_consumed);
    }

    // After 10 iterations: received 1000, consumed 500, remainder = 500
    EXPECT_EQ(rb.view().size(), 500);
}

} // namespace sl::http::v1::detail
