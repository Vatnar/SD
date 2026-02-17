#pragma once
#include "Core/types.hpp"


namespace SD {
struct Entity {
  u32 index;
  u32 generation;

  bool operator==(Entity other) const {
    return index == other.index && generation == other.generation;
  }
};
} // namespace SD
