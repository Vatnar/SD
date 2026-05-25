#pragma once
#include <cstdint>
#include <functional>

namespace SD {

struct ViewId {
  uint32_t value = 0;

  constexpr ViewId(uint32_t v = 0) noexcept : value(v) {}

  constexpr ViewId& operator++() noexcept {
    ++value;
    return *this;
  }
  constexpr ViewId operator++(int) noexcept {
    ViewId tmp = *this;
    ++value;
    return tmp;
  }

  constexpr auto operator<=>(const ViewId&) const = default;
  constexpr bool operator==(const ViewId&) const = default;

  explicit constexpr operator uint32_t() const noexcept { return value; }
};

struct WindowId {
  uint32_t value = 0;

  constexpr WindowId(uint32_t v = 0) noexcept : value(v) {}

  constexpr WindowId& operator++() noexcept {
    ++value;
    return *this;
  }
  constexpr WindowId operator++(int) noexcept {
    WindowId tmp = *this;
    ++value;
    return tmp;
  }

  constexpr auto operator<=>(const WindowId&) const = default;
  constexpr bool operator==(const WindowId&) const = default;

  explicit constexpr operator uint32_t() const noexcept { return value; }
};

} // namespace SD

// std::hash specializations (inline to avoid ODR violations across TUs)
template<>
struct std::hash<SD::ViewId> {
  inline std::size_t operator()(SD::ViewId id) const noexcept {
    return std::hash<uint32_t>{}(id.value);
  }
};

template<>
struct std::hash<SD::WindowId> {
  inline std::size_t operator()(SD::WindowId id) const noexcept {
    return std::hash<uint32_t>{}(id.value);
  }
};
