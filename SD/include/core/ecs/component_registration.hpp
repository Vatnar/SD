/**
 * @file Component.hpp
 * @brief Component registration and trait utilities for ECS
 *
 * @note This ECS implementation is single-threaded. All EntityManager and component
 *       operations must occur on the same thread. The ComponentIdGenerator uses
 *       static storage which is not thread-safe for concurrent registration.
 *
 * @section ComponentRegistration Component Registration
 *
 * To create a new component:
 * 1. Define the struct as pure data (no methods)
 * 2. Use REGISTER_SD_COMPONENT(Type) macro to register with ECS
 * 3. Specialize ComponentSerializer<Type> to implement Serialize/Deserialize
 *
 * Example:
 * @code
 * struct MyComponent {
 *     float value;
 *     int count;
 * };
 * REGISTER_SD_COMPONENT(MyComponent);
 *
 * template<>
 * struct ComponentSerializer<MyComponent> {
 *     static void Serialize(const MyComponent& c, Serializer& s) {
 *         s.Write(c.value);
 *         s.Write(c.count);
 *     }
 *     static void Deserialize(MyComponent& c, Serializer& s) {
 *         c.value = s.Read<float>();
 *         c.count = s.Read<int>();
 *     }
 * };
 * @endcode
 */
#pragma once

#include <core/types.hpp>
#include <utils/serialization.hpp>

namespace sd {
template<typename... Ts>
struct ComponentGroup {};

template<typename T>
struct ComponentTraits;

template<typename T>
struct ComponentSerializer {
  static void serialize(const T& component, Serializer& s) = delete;
  static void deserialize(T& component, Serializer& s) = delete;
};

namespace detail {
struct ComponentIdGenerator {
  ComponentIdGenerator() = delete;
  ~ComponentIdGenerator() = delete;
  ComponentIdGenerator(const ComponentIdGenerator&) = delete;
  ComponentIdGenerator& operator=(const ComponentIdGenerator&) = delete;

  static usize next() { return m_counter++; }

private:
  static inline usize m_counter = 0;
};

} // namespace detail

template<typename T>
struct ComponentTraits {
  static constexpr bool s_is_registered = false;
};

/**
 * @brief Registers a SD component for use with ECS
 * @details Needs to be run to use a component with ECS
 * @param Type struct type
 */
#define REGISTER_SD_COMPONENT(Type)                                          \
  template<>                                                                 \
  struct ComponentTraits<Type> {                                             \
    static constexpr bool s_is_registered = true;                               \
    static constexpr const char* name = "SD_" #Type;                         \
    static inline const usize id = detail::ComponentIdGenerator::next(); \
  };

/**
 * @brief Registers a component for use with ECS (non-namespaced)
 * @param Type struct type
 */
#define REGISTER_COMPONENT(Type)                                             \
  template<>                                                                 \
  struct ComponentTraits<Type> {                                             \
    static constexpr bool s_is_registered = true;                               \
    static constexpr const char* name = #Type;                               \
    static inline const usize id = detail::ComponentIdGenerator::next(); \
  };

struct ComponentDebugInfo {
  usize id;
  const char* name;
  void* data;
};

template<typename T>
concept SerializableComponent = requires(T& t, Serializer& s) {
  ComponentSerializer<T>::serialize(t, s);
  ComponentSerializer<T>::deserialize(t, s);
};

} // namespace SD
