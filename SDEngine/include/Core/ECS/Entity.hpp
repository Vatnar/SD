#pragma once
#include <cstdint>


struct Entity {
  uint32_t index;
  uint32_t generation;

  bool operator==(Entity other) const {
    return index == other.index && generation == other.generation;
  }
};
