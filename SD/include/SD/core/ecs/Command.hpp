#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "EntityManager.hpp"
#include "SD/core/type_id.hpp"
#include "SD/core/types.hpp"
#include "SD/utils/serialization.hpp"

namespace sd {

class CommandQueue;

struct EntityHandle {
  U32 id;

  [[nodiscard]] bool is_valid() const { return id != 0; }

  bool operator==(const EntityHandle& other) const { return id == other.id; }
  bool operator!=(const EntityHandle& other) const { return !(*this == other); }
};

struct CommandNode {
  U64   type_id;
  void* data;

  void (*execute_fn)(void* data, EntityManager<ComponentGroup<>>& em, CommandQueue& queue);
  void (*serialize_fn)(void* data, Serializer& s);
  void (*deserialize_fn)(void* data, Serializer& s);
};

} // namespace sd

template<>
struct std::hash<sd::EntityHandle> {
  inline std::size_t operator()(sd::EntityHandle h) const noexcept {
    return std::hash<U32>{}(h.id);
  }
};
