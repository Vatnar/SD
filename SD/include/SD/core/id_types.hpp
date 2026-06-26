#pragma once
#include <cstdint>
#include <functional>

namespace sd {

struct ViewId {
  uint32_t value = 0;

  ViewId() = default;
  explicit constexpr ViewId(uint32_t v) noexcept : value(v) {}

  constexpr ViewId& operator++() noexcept {
    ++value;
    return *this;
  }
  constexpr ViewId operator++(int) noexcept {
    const ViewId tmp = *this;
    ++value;
    return tmp;
  }

  constexpr auto operator<=>(const ViewId&) const = default;
  constexpr bool operator==(const ViewId&) const  = default;

  explicit constexpr operator uint32_t() const noexcept { return value; }
};

struct WindowId {
  uint32_t value = 0;

  WindowId() = default;
  explicit constexpr WindowId(uint32_t v) noexcept : value(v) {}

  constexpr WindowId& operator++() noexcept {
    ++value;
    return *this;
  }
  constexpr WindowId operator++(int) noexcept {
    const WindowId tmp = *this;
    ++value;
    return tmp;
  }

  constexpr auto operator<=>(const WindowId&) const = default;
  constexpr bool operator==(const WindowId&) const  = default;

  explicit constexpr operator uint32_t() const noexcept { return value; }
};

} // namespace sd

// std::hash specializations (inline to avoid ODR violations across TUs)
template<>
struct std::hash<sd::ViewId> {
  inline std::size_t operator()(const sd::ViewId id) const noexcept {
    return std::hash<uint32_t>{}(id.value);
  }
};

template<>
struct std::hash<sd::WindowId> {
  inline std::size_t operator()(const sd::WindowId id) const noexcept {
    return std::hash<uint32_t>{}(id.value);
  }
};
