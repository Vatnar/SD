/**
 * @file Component.hpp
 * @brief Component registration and trait utilities for ECS
 *
 * @note This ECS implementation is single-threaded. All EntityManager and component
 *       operations must occur on the same thread. The ComponentIdGenerator uses
 *       static storage which is not thread-safe for concurrent registration.
 */
#pragma once

#include <Core/types.hpp>
#include <cstddef>
#include <tuple>

namespace SD {
template<typename... Ts>
struct ComponentGroup {};

template<typename T>
struct ComponentTraits;

namespace detail {
/**
 * @internal Implementation detail
 * @brief Generates component ids, used internally by ComponentTraits
 */
struct ComponentIdGenerator {
  ComponentIdGenerator() = delete;
  ~ComponentIdGenerator() = delete;
  ComponentIdGenerator(const ComponentIdGenerator&) = delete;
  ComponentIdGenerator& operator=(const ComponentIdGenerator&) = delete;

  static usize Next() { return counter++; }

private:
  static inline usize counter = 0;
};

} // namespace detail
/**
 * @brief Traits for a component, Name, and Id
 * @details Use @ref REGISTER_SD_COMPONENT or @ref REGISTER_COMPONENT macro to properly define the
 * correct traits
 * @tparam T Pure data struct
 */
template<typename T>
struct ComponentTraits {
  static constexpr bool IsRegistered = false;
};

/**
 * @brief Registers a SD component for use with ECS
 * @details Needs to be run to use a component with ECS
 * @param Type struct type
 */
#define REGISTER_SD_COMPONENT(Type)                                          \
  template<>                                                                 \
  struct ComponentTraits<Type> {                                             \
    static constexpr bool IsRegistered = true;                               \
    static constexpr const char* Name = "SD_" #Type;                         \
    static inline const usize Id = SD::detail::ComponentIdGenerator::Next(); \
  };

/**
 * @brief Registers a component for use with ECS
 * @details Needs to be run to use a component with ECS
 * @param Type struct type
 */
#define REGISTER_COMPONENT(Type)                                             \
  template<>                                                                 \
  struct ComponentTraits<Type> {                                             \
    static constexpr bool IsRegistered = true;                               \
    static constexpr const char* Name = #Type;                               \
    static inline const usize Id = SD::detail::ComponentIdGenerator::Next(); \
  };

struct ComponentDebugInfo {
  usize id;
  const char* name;
  void* data;
};
} // namespace SD
