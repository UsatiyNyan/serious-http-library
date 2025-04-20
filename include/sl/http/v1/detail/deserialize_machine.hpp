//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/match/pmatch.hpp>
#include <sl/meta/traits/is_specialization.hpp>

#include <libassert/assert.hpp>

#include <cstddef>
#include <span>
#include <variant>
#include <vector>

namespace sl::http::v1::detail::deserialize {

template <typename AllocT = std::allocator<std::byte>>
struct remainder_buffer {
    using buffer_type = std::vector<std::byte, AllocT>;

public:
    explicit remainder_buffer(const AllocT& alloc = AllocT{}) : buffer_{ alloc } {}

    [[nodiscard]] std::span<const std::byte> view() const {
        ASSERT(buffer_.size() >= offset_);
        return std::span{ buffer_ }.subspan(offset_);
    }

    [[nodiscard]] std::size_t merge(std::span<const std::byte> byte_buffer) {
        const std::size_t offset = std::exchange(offset_, 0);
        if (buffer_.size() == offset) {
            buffer_.assign(byte_buffer.begin(), byte_buffer.end());
        } else {
            ASSERT(buffer_.size() > offset);
            using difference_type = typename buffer_type::difference_type;
            buffer_.erase(buffer_.begin(), std::next(buffer_.begin(), static_cast<difference_type>(offset)));
            buffer_.insert(buffer_.end(), byte_buffer.begin(), byte_buffer.end());
        }
        return offset;
    }

    void add_offset(std::size_t offset) {
        ASSERT(offset_ + offset <= buffer_.size());
        offset_ += offset;
    }

private:
    buffer_type buffer_{};
    std::size_t offset_ = 0;
};

template <typename T>
concept Variant = meta::is_specialization_v<T, std::variant>;

template <typename StateT>
concept State = requires(StateT&& state, std::span<const std::byte> byte_buffer) {
    { std::move(state).process(byte_buffer) } /* -> (StateVariant StateVariantT) => (StateVariant, size_t, bool) */;
};

// TODO: visited_bytes
template <Variant StateVariantT>
struct process_result {
    static process_result ok(StateVariantT state_variant, std::size_t offset) {
        return process_result{
            .state_variant = std::move(state_variant),
            .offset = offset,
            .can_continue = true,
        };
    }

    static process_result err(StateVariantT state_variant) {
        return process_result{
            .state_variant = std::move(state_variant),
            .offset = 0,
            .can_continue = false,
        };
    }

public:
    StateVariantT state_variant;
    std::size_t offset;
    bool can_continue;
};

template <Variant StateVariantT, typename AllocT = std::allocator<std::byte>>
struct [[nodiscard]] machine {
    using state_process_result_type = process_result<StateVariantT>;

public:
    machine process(std::span<const std::byte> byte_buffer) && {
        // less allocations and copying
        if (remainder.view().empty()) {
            while (!byte_buffer.empty()) {
                state_process_result_type result =
                    std::move(state_variant) | meta::pmatch{ [byte_buffer](State auto&& state) {
                        return std::move(state).process(byte_buffer);
                    } };
                state_variant = std::move(result.state_variant);
                byte_buffer = byte_buffer.subspan(result.offset);
                if (!result.can_continue) {
                    break;
                }
            }
        }

        std::ignore = remainder.merge(byte_buffer);

        while (true) {
            state_process_result_type result =
                std::move(state_variant) | meta::pmatch{ [remainder_view = remainder.view()](State auto&& state) {
                    return std::move(state).process(remainder_view);
                } };
            state_variant = std::move(result.state_variant);
            remainder.add_offset(result.offset);
            if (!result.can_continue) {
                break;
            }
        }

        return machine{
            .state_variant = std::move(state_variant),
            .remainder = std::move(remainder),
        };
    }

public:
    StateVariantT state_variant;
    remainder_buffer<AllocT> remainder;
};

} // namespace sl::http::v1::detail::deserialize
