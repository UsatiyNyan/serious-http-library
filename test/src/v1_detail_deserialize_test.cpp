//
// Created by usatiynyan.
//

#include "sl/http/v1/detail/deserialize_machine.hpp"
#include "sl/http/v1/detail/strings.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <random>
#include <variant>

namespace sl::http::v1::detail::deserialize {

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
    remainder_buffer rb;
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

// MACHINE ----

struct state_empty;
struct state_intermediate;
struct state_complete;

using state_variant = std::variant< //
    state_empty,
    state_intermediate,
    state_complete>;

struct state_empty {
    process_result<state_variant> process(std::span<const std::byte> byte_buffer) &&;
};

struct state_intermediate {
    static constexpr std::string_view token = " ";

public:
    process_result<state_variant> process(std::span<const std::byte> byte_buffer) &&;

public:
    std::string_view intermediate;
};

struct state_complete {
    static constexpr std::string_view token = "\n\n";

public:
    process_result<state_variant> process(std::span<const std::byte> byte_buffer) &&;

public:
    std::string_view intermediate;
    std::string_view complete;
};

process_result<state_variant> state_empty::process(std::span<const std::byte> byte_buffer) && {
    const auto str_buffer = buffer_byte_to_str(byte_buffer);
    const auto intermediate_result = try_find_unlimited(str_buffer, state_intermediate::token);
    if (!intermediate_result.has_value()) {
        return process_result<state_variant>::err(std::move(*this));
    }
    const auto intermediate = intermediate_result.value();
    return process_result<state_variant>::ok(
        state_intermediate{
            .intermediate = intermediate.value,
        },
        intermediate.offset
    );
}

process_result<state_variant> state_intermediate::process(std::span<const std::byte> byte_buffer) && {
    const auto str_buffer = buffer_byte_to_str(byte_buffer);
    const auto complete_result = try_find_unlimited(str_buffer, state_complete::token);
    if (!complete_result.has_value()) {
        return process_result<state_variant>::err(std::move(*this));
    }
    const auto complete = complete_result.value();
    return process_result<state_variant>::ok(
        state_complete{
            .intermediate = intermediate,
            .complete = complete.value,
        },
        complete.offset
    );
}

process_result<state_variant> state_complete::process(std::span<const std::byte>) && {
    return process_result<state_variant>::err(std::move(*this));
}

struct Machine : ::testing::Test {
    void SetUp() override {
        allocator_state = {};
        m.emplace(state_empty{}, remainder_buffer{ allocator });
    }
    void TearDown() override { m.reset(); }

protected:
    logging_allocator_state allocator_state{};
    logging_allocator<std::byte> allocator{ allocator_state };
    meta::maybe<machine<state_variant, logging_allocator<std::byte>>> m;
};

TEST_F(Machine, empty) {
    auto result = std::move(*m).process(buffer_str_to_byte(""));
    EXPECT_EQ(allocator_state.alloc_counter, 0);
    EXPECT_EQ(allocator_state.dealloc_counter, 0);
    EXPECT_TRUE(result.remainder.view().empty());

    EXPECT_TRUE(std::holds_alternative<state_empty>(result.state_variant));
}

TEST_F(Machine, intermediateOptimized) {
    auto result = std::move(*m).process(buffer_str_to_byte("intermediate42 "));
    EXPECT_EQ(allocator_state.alloc_counter, 0);
    EXPECT_EQ(allocator_state.dealloc_counter, 0);
    EXPECT_TRUE(result.remainder.view().empty());

    const auto* s_i = std::get_if<state_intermediate>(&result.state_variant);
    ASSERT_TRUE(s_i);
    EXPECT_EQ(s_i->intermediate, "intermediate42");
}

TEST_F(Machine, completeOptimized) {
    auto result = std::move(*m).process(buffer_str_to_byte("42intermediate complete13\n\n"));
    EXPECT_EQ(allocator_state.alloc_counter, 0);
    EXPECT_EQ(allocator_state.dealloc_counter, 0);
    EXPECT_TRUE(result.remainder.view().empty());

    const auto* s_c = std::get_if<state_complete>(&result.state_variant);
    ASSERT_TRUE(s_c);
    EXPECT_EQ(s_c->intermediate, "42intermediate");
    EXPECT_EQ(s_c->complete, "complete13");
}

TEST_F(Machine, intermediatePartial) {
    constexpr std::string_view first_part = "42interme";
    m = std::move(*m).process(buffer_str_to_byte(first_part));
    const auto first_alloc_counter = allocator_state.alloc_counter;
    EXPECT_GE(first_alloc_counter, first_part.size());
    EXPECT_EQ(buffer_byte_to_str(m->remainder.view()), first_part);
    EXPECT_TRUE(std::holds_alternative<state_empty>(m->state_variant));

    constexpr std::string_view second_part = "diate ";
    m = std::move(*m).process(buffer_str_to_byte(second_part));
    EXPECT_GE(allocator_state.alloc_counter, first_alloc_counter);
    EXPECT_TRUE(m->remainder.view().empty());

    const auto* s_i = std::get_if<state_intermediate>(&m->state_variant);
    ASSERT_TRUE(s_i);
    EXPECT_EQ(s_i->intermediate, "42intermediate");
}

TEST_F(Machine, completePartial) {
    m = std::move(*m).process(buffer_str_to_byte("42intermediate comp"));
    const auto first_alloc_counter = allocator_state.alloc_counter;
    EXPECT_GE(first_alloc_counter, std::string_view("comp").size());
    EXPECT_EQ(buffer_byte_to_str(m->remainder.view()), "comp");
    EXPECT_TRUE(std::holds_alternative<state_intermediate>(m->state_variant));

    m = std::move(*m).process(buffer_str_to_byte("lete13\n\n"));
    EXPECT_GE(allocator_state.alloc_counter, first_alloc_counter);
    EXPECT_TRUE(m->remainder.view().empty());

    const auto* s_c = std::get_if<state_complete>(&m->state_variant);
    ASSERT_TRUE(s_c);
    EXPECT_EQ(s_c->intermediate, "42intermediate");
    EXPECT_EQ(s_c->complete, "complete13");
}

TEST_F(Machine, intermediateSlow) {
    constexpr std::string_view str_buffer = "42intermediate";
    for (std::size_t i = 0; i < str_buffer.size(); ++i) {
        const auto chunk = str_buffer.substr(i, 1);
        m = std::move(*m).process(buffer_str_to_byte(chunk));
        EXPECT_FALSE(std::holds_alternative<state_intermediate>(m->state_variant));
        EXPECT_EQ(buffer_byte_to_str(m->remainder.view()), str_buffer.substr(0, i + 1));
    }
    m = std::move(*m).process(buffer_str_to_byte(" "));

    const auto* s_i = std::get_if<state_intermediate>(&m->state_variant);
    ASSERT_TRUE(s_i);
    EXPECT_EQ(s_i->intermediate, str_buffer);
}

TEST_F(Machine, completeSlow) {}

TEST_F(Machine, intermediateAfterComplete) {}

TEST_F(Machine, completeAfterComplete) {}

TEST_F(Machine, manyCompleteRandomResidueSize) {}

} // namespace sl::http::v1::detail::deserialize
