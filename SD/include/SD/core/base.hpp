#pragma once

#include <meta>
#include <ostream>
#include <string_view>
#include <type_traits>
#if defined(_MSC_VER)
#define TRAP() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define TRAP() __builtin_trap()
#else
#error Unknown trap intrinsic for compiler.
#endif
#define ASSERT_ALWAYS(x) \
  do {                   \
    if (!(x)) {          \
      TRAP();            \
    }                    \
  } while (0)

#ifdef SD_DEBUG
#define ASSERT(x) ASSERT_ALWAYS(x)
#else
#define ASSERT(x) (void)(x)
#endif
#define INVALID_PATH    ASSERT(!"Invalid Path!")
#define NOT_IMPLEMENTED ASSERT(!"Not Implemented!")
#define NO_OP           ((void)0)

#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE [[gnu::always_inline]] inline
#else
#error "Not defined for your compiler"
#endif


//~ BITMASK ENUM OPERATIONS
template<typename T>
struct sdIsBitmaskEnum : std::false_type {};
template<typename T> concept BitmaskEnum = sdIsBitmaskEnum<T>::value;

#define BITMASK_ENUM(Type)                                                            \
  static_assert(std::is_enum_v<Type>, "BITMASK_ENUM can only be used on enum types"); \
  template<>                                                                          \
  struct sdIsBitmaskEnum<Type> : std::true_type {};

template<BitmaskEnum T>
struct BitmaskResult {
  using U = std::underlying_type_t<T>;
  U value{};

  FORCE_INLINE constexpr operator bool() const noexcept { return value != 0; }         // NOLINT
  FORCE_INLINE constexpr operator T() const noexcept { return static_cast<T>(value); } // NOLINT
};
template<BitmaskEnum T>
FORCE_INLINE constexpr BitmaskResult<T> operator&(T lhs, T rhs) {
  using U = std::underlying_type_t<T>;
  return {static_cast<U>(lhs) & static_cast<U>(rhs)};
}

template<BitmaskEnum T>
FORCE_INLINE constexpr BitmaskResult<T> operator|(T lhs, T rhs) {
  using U = std::underlying_type_t<T>;
  return {static_cast<U>(lhs) | static_cast<U>(rhs)};
}

template<BitmaskEnum T>
FORCE_INLINE constexpr BitmaskResult<T> operator~(T rhs) {
  using U = std::underlying_type_t<T>;
  return {static_cast<U>(~static_cast<U>(rhs))};
}

template<BitmaskEnum T>
FORCE_INLINE constexpr T& operator|=(T& lhs, T rhs) {
  using U = std::underlying_type_t<T>;
  lhs     = static_cast<T>(static_cast<U>(lhs) | static_cast<U>(rhs));
  return lhs;
}

template<BitmaskEnum T>
FORCE_INLINE constexpr T& operator&=(T& lhs, T rhs) {
  using U = std::underlying_type_t<T>;
  lhs     = static_cast<T>(static_cast<U>(lhs) & static_cast<U>(rhs));
  return lhs;
}

//~ AUTOMATIC ENUM STRING CONVERSION
template<typename T>
struct BitmaskStreamer {
  T value;

  friend std::ostream& operator<<(std::ostream& os, BitmaskStreamer wrapper) {
    using U         = std::underlying_type_t<T>;
    U numeric_value = static_cast<U>(wrapper.value);

    if (numeric_value == 0) {
      return os << "None";
    }

    static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^T));
    bool                  first       = true;

    template for (constexpr auto e : enumerators) {
      constexpr U flag_val = static_cast<U>([:e:]);
      if constexpr (flag_val != 0) {
        if ((numeric_value & flag_val) == flag_val) {
          if (!first)
            os << " | ";
          first = false;
          os << std::meta::identifier_of(e);
        }
      }
    }
    return os;
  }
};

template<typename T>
  requires std::is_enum_v<T> && (!sdIsBitmaskEnum<T>::value)
[[nodiscard]] FORCE_INLINE constexpr std::string_view enum_to_string(T value) noexcept {
  static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^T));

  template for (constexpr auto e : enumerators) {
    if (value == [:e:]) {
      return std::meta::identifier_of(e);
    }
  }
  return "Unknown";
}

template<typename T>
  requires BitmaskEnum<T>
[[nodiscard]] FORCE_INLINE constexpr BitmaskStreamer<T> enum_to_string(T value) noexcept {
  return {value};
}


//~ SourceLocation
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && (_MSC_VER >= 1926))
#define SD_FILE __builtin_FILE()
#define SD_LINE __builtin_LINE()
#else
#error "__buitin_FILE() not defined for your compiler"
#endif


namespace sd {
struct SourceLocation {
  const char* file;
  unsigned    line;

  static consteval SourceLocation current(const char* file = __builtin_FILE(),
                                          unsigned    line = __builtin_LINE()) {
    return {file, line};
  }
};
} // namespace sd
