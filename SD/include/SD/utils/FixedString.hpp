#pragma once
#include <algorithm>
#include <array>
#include <string_view>

#include <fmt/core.h>

#include "SD/core/types.hpp"

// todo: move
void compile_time_assert_failed(
    const char* msg); // a bit ugly, but since we dont have exceptions on, we could possibly turn on
// exceptions for compile time stuff, but that makes it harder to catch
// accidental exceptions in other stuff


template<usize MAX_SIZE>
struct FixedString {
  std::array<char, MAX_SIZE> data{};
  usize                      size{};
  constexpr FixedString() = default;
  consteval explicit FixedString(std::string_view sv) {
    if (sv.size() > MAX_SIZE)
      compile_time_assert_failed("Provided string doesnt fit in type");

    std::ranges::copy_n(
        sv.begin(),
        sv.size(),
        data.begin()); // i get no matchin function to call for typpe const __copy_n_fn
    size = sv.size();
  }

  consteval explicit FixedString(const char ch) {
    data = {ch};
    size = 1;
  }
  constexpr explicit operator std::string_view() const {
    return std::string_view(data.data(), size);
  }
};
template<usize N>
struct fmt::formatter<FixedString<N>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const FixedString<N>& value, format_context& ctx) {
    return std::copy(value.data.begin(),
                     value.data.begin() + static_cast<std::ptrdiff_t>(value.size),
                     ctx.out());
  }
};

// NOTE: We dont want + operator to create new types automatically, we rather that to be explicit
//  when needed
template<std::size_t MAX_SIZE>
consteval auto operator+(FixedString<MAX_SIZE> lhs, FixedString<MAX_SIZE> rhs) {
  auto new_size = lhs.size + rhs.size;
  if (new_size > MAX_SIZE)
    compile_time_assert_failed("Sizes of lhs and rhs cant exceed type MAX_SIZE");
  FixedString<MAX_SIZE> buffer{};

  std::ranges::copy_n(lhs.data.begin(), lhs.size, buffer.data.begin());
  std::ranges::copy_n(rhs.data.begin(), rhs.size, buffer.data.begin() + lhs.size);
  buffer.size = new_size;

  return buffer;
}
