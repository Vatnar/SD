#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "core/types.hpp"
#include "SparseEntitySet.hpp"

namespace sd {

class ComponentFactory {
public:
  using PoolCreatorFn = std::function<std::unique_ptr<SparseEntitySetBase>()>;

  static void register_component(u32 component_id, PoolCreatorFn creator);
  static std::unique_ptr<SparseEntitySetBase> create(u32 component_id);
  static bool is_registered(u32 component_id);

private:
  static inline std::vector<PoolCreatorFn> m_creators;
};

#define REGISTER_SERIALIZABLE_COMPONENT(T)                                 \
  namespace {                                                              \
    struct T## PoolRegistrar {                                             \
      T## PoolRegistrar() {                                                \
        ComponentFactory::register_component(ComponentTraits<T>::id,       \
          [] { return std::make_unique<SparseEntitySet<T>>(); });        \
      }                                                                    \
    };                                                                     \
    static T## PoolRegistrar T## s_pool_registrar;                         \
  }

} // namespace sd