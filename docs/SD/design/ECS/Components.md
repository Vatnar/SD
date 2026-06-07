---
date: 2026-04-06
---
Each registered component has its own SparseEntitySet pool — isolation between types and sequential storage per type for fast system queries.

Components are pure data structs with no behaviour. Registration is done via a macro:

```cpp
struct Transform {
    VLA::Matrix4x4f world_matrix;
};
REGISTER_SD_COMPONENT(Transform);
```

The macro specialises `ComponentTraits<T>` with:
- `s_is_registered = true` — required for compile-time checks
- `name` — human-readable name for debugging and serialization
- `id` — a monotonically increasing unique ID assigned by `ComponentIdGenerator`

```cpp
#define REGISTER_SD_COMPONENT(Type)                                          \
  template<>                                                                 \
  struct ComponentTraits<Type> {                                             \
    static constexpr bool        s_is_registered = true;                     \
    static constexpr const char* name            = "SD_" #Type;              \
    static inline const usize    id              = detail::ComponentIdGenerator::next(); \
  };
```

**Two variants exist:**
- `REGISTER_SD_COMPONENT` — prepends `"SD_"` to the name (for engine-built components)
- `REGISTER_COMPONENT` — uses the raw type name as-is (for user-defined components)

**Serialization:** Components that need persistence specialise `ComponentSerializer<T>`. This is useful for saving/loading and also networking.

```cpp
template<>
struct ComponentSerializer<Transform> {
    static void serialize(const Transform& c, Serializer& s) {
        s.write(c.world_matrix.A);
    }
    static void deserialize(Transform& c, Serializer& s) {
        s.read(c.world_matrix.A);
    }
};
```

> [!warning] Components
> Since rendering and physics, and more. Is not implemented yet, the components are very much WIP, and more components will be added, and existing components will be changed.
