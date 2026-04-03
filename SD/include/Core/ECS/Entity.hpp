// TODO(docs): Add file-level Doxygen header
//   - @file Entity.hpp
//   - @brief Brief description of Entity concept
//   - Overview of the entity-identifier system (index + generation)
//   - Example usage in context of ECS
#pragma once
#include "Core/types.hpp"


namespace SD {
// TODO(docs): Document Entity struct
//   - Explain index vs generation semantics
//   - Describe entity lifecycle (created -> alive -> destroyed -> recycled)
//   - Note about generation preventing use-after-free
//   - Example: Entity e = manager.Create(); // e = {index: 0, generation: 0}
struct Entity {
  u32 index;
  u32 generation;

  bool operator==(Entity other) const {
    return index == other.index && generation == other.generation;
  }
};
} // namespace SD
