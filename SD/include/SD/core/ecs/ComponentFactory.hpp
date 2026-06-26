#pragma once

#include <vector>

#include "ComponentPoolNode.hpp"
#include "SD/arena.hpp"
#include "SD/core/types.hpp"
#include "SD/export.hpp"

namespace sd {

struct SD_EXPORT ComponentFactory {
  using PoolNodeCreatorFn = ComponentPoolNode (*)(Arena* arena);

  static void              register_component(U32 component_id, PoolNodeCreatorFn creator);
  static ComponentPoolNode create(U32 component_id, Arena* arena);
  static bool              is_registered(U32 component_id);

  static void clear();
  static void register_default_pools();


  static inline std::vector<PoolNodeCreatorFn> m_creators;
};

} // namespace sd
