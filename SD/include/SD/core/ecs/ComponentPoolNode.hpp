#pragma once

#include "Entity.hpp"

namespace sd {

struct Serializer;

struct ComponentPoolNode {
  void* pool = nullptr;

  bool (*remove_fn)(void* pool, Entity e)           = nullptr;
  void (*serialize_fn)(void* pool, Serializer& s)   = nullptr;
  void (*deserialize_fn)(void* pool, Serializer& s) = nullptr;
};

} // namespace sd
