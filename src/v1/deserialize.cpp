//
// Created by usatiynyan.
//

#include "sl/http/v1/deserialize.hpp"

#include <sl/meta/match/pmatch.hpp>

#include <libassert/assert.hpp>

namespace sl::http::v1::deserialize {

meta::maybe<request_parser::result_type> request_parser::process(std::span<const std::byte> byte_buffer) & {
    m_ = std::move(m_).process(byte_buffer);
    auto maybe_result = std::get_if<state_complete>(&m_.state_variant)
                        | meta::pmatch{
                              [](state_complete&& a_state_complete) -> meta::maybe<result_type> {
                                  return std::move(a_state_complete.result);
                              },
                              []() -> meta::maybe<result_type> { return meta::null; },
                          };
    if (maybe_result.has_value()) {
        m_.state_variant = state_empty{};
    }
    return maybe_result;
}

} // namespace sl::http::v1::deserialize
