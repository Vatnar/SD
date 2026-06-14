#pragma once

#include <meta>

#include "SD/core/types.hpp"
#include "SD/utils/FixedString.hpp"
#include "SD/utils/serialization.hpp"
// TODO: REMOVE< just for test

namespace sd {
template<typename... Ts>
struct ComponentGroup {};


template<typename>
inline constexpr bool k_always_false = false;

template<typename T, typename Group>
struct ComponentTraits;

template<typename A, typename B>
struct ConcatComponentGroups;

template<typename... As, typename... Bs>
struct ConcatComponentGroups<ComponentGroup<As...>, ComponentGroup<Bs...>> {
  using type = ComponentGroup<As..., Bs...>;
};

template<typename A, typename B>
using ConcatComponentGroups_t = typename ConcatComponentGroups<A, B>::type;

template<typename Group>
struct IsUniqueComponentGroup;

template<>
struct IsUniqueComponentGroup<ComponentGroup<>> : std::true_type {};

template<typename T, typename... Ts>
struct IsUniqueComponentGroup<ComponentGroup<T, Ts...>>
  : std::bool_constant<(!(std::same_as<T, Ts> || ...)) &&
                       IsUniqueComponentGroup<ComponentGroup<Ts...>>::value> {};


template<typename T, typename... Ts>
struct ComponentTraits<T, ComponentGroup<Ts...>> {
  static constexpr bool is_registered = (std::same_as<T, Ts> || ...);

  static constexpr USize id() {
    constexpr bool matches[] = {std::same_as<T, Ts>...};
    for (USize i = 0; i < sizeof...(Ts); ++i) {
      if (matches[i])
        return i;
    }
    return static_cast<USize>(-1);
  }

  static constexpr auto name = std::meta::identifier_of(^^T);
};

// TODO: REFLECATION SERIALIZATION
template<typename T>
struct ComponentSerializer {
  static void serialize(const T& component, Serializer& s) = delete;
  static void deserialize(T& component, Serializer& s)     = delete;
};

template<typename T>
concept SerializableComponent = requires(T& t, Serializer& s) {
  ComponentSerializer<T>::serialize(t, s);
  ComponentSerializer<T>::deserialize(t, s);
};

} // namespace sd
