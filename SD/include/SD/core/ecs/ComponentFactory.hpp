#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "SD/core/types.hpp"
#include "SD/export.hpp"
#include "SparseEntitySet.hpp"

namespace sd {

class SD_EXPORT ComponentFactory {
public:
  using PoolCreatorFn = std::function<std::unique_ptr<SparseEntitySetBase>()>;

  static void register_component(u32 component_id, PoolCreatorFn creator);
  static std::unique_ptr<SparseEntitySetBase> create(u32 component_id);
  static bool                                 is_registered(u32 component_id);

  // Clears all registrations and resets the ID counter.
  // register_default_pools() re-registers the engine's built-in components
  // (Transform, Camera, Renderable, DebugName). Used on full game restart
  // to prevent stale lambdas from a previously dlclosed .so.
  static void clear();
  static void register_default_pools();

private:
  static inline std::vector<PoolCreatorFn> m_creators;
};

#define REGISTER_SERIALIZABLE_COMPONENT(T)                                                         \
  namespace {                                                                                      \
  struct T##PoolRegistrar {                                                                        \
    T##PoolRegistrar() {                                                                           \
      ComponentFactory::register_component(ComponentTraits<T>::id,                                 \
                                           [] { return std::make_unique<SparseEntitySet<T>>(); }); \
    }                                                                                              \
  };                                                                                               \
  static T##PoolRegistrar T##s_pool_registrar;                                                     \
  }

} // namespace sd
