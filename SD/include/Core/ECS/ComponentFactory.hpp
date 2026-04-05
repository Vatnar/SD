#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Core/types.hpp"
#include "SparseEntitySet.hpp"

namespace SD {

class ComponentFactory {
public:
  using PoolCreatorFunc = std::function<std::unique_ptr<SparseEntitySetBase>()>;

  static void Register(u32 componentId, PoolCreatorFunc creator);
  static std::unique_ptr<SparseEntitySetBase> Create(u32 componentId);
  static bool IsRegistered(u32 componentId);

private:
  static inline std::vector<PoolCreatorFunc> creators;
};

#define REGISTER_SERIALIZABLE_COMPONENT(T)                                 \
  namespace {                                                                \
    struct T##PoolRegistrar {                                               \
      T##PoolRegistrar() {                                                  \
        SD::ComponentFactory::Register(SD::ComponentTraits<T>::Id,        \
          [] { return std::make_unique<SD::SparseEntitySet<T>>(); });       \
      }                                                                     \
    };                                                                      \
    static T##PoolRegistrar T##poolRegistrar;                               \
  }

} // namespace SD