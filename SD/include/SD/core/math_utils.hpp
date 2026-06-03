#pragma once
namespace sd {

/**
 * @namespace math
 * @brief Math utilities
 */
namespace math {
// TODO(docs): Document log2_int - explain use case, constraints (what happens with 0)
consteval usize log2_int(std::unsigned_integral auto n) {
  // log2(n) = bit_width(n) - 1
  return n == 0 ? 0 : std::bit_width(n) - 1;
}
} // namespace math


} // namespace sd
