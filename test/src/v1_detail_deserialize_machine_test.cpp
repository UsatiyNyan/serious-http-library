//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/machine.hpp"

#include <gtest/gtest.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>

#include <cstring>
#include <random>

namespace sl::http::v1::deserialize::detail {

inline bool span_eq(std::span<const std::byte> a, std::span<const std::byte> b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0;
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
    remainder_buffer rb{ allocator };

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
    remainder_buffer rb{ allocator };

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
    remainder_buffer rb{ allocator };

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

} // namespace sl::http::v1::deserialize::detail
