//
// Created by usatiynyan.
//

#pragma once

#include <sl/meta/match/pmatch.hpp>
#include <sl/meta/traits/is_specialization.hpp>

#include <sl/meta/assert.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace sl::http::v1::detail {

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

} // namespace sl::http::v1::detail

namespace sl::http::v1::deserialize::detail {

using v1::detail::remainder_buffer;

template <typename T>
struct parse_result {
    static parse_result more(T value, std::size_t offset) {
        return parse_result{
            .value = std::move(value),
            .offset = offset,
        };
    }

    static parse_result stop(T value) {
        return parse_result{
            .value = std::move(value),
            .offset = 0,
        };
    }

public:
    T value;
    std::size_t offset;
};

} // namespace sl::http::v1::deserialize::detail
