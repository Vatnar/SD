---
date: 2026-04-06
---
Each registered component has its own SparseEntitySet pool — isolation between types and sequential storage per type for fast system queries.

Components are pure data structs with no behaviour. Registration is done via a macro:

```cpp
struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};
REGISTER_SD_COMPONENT(Transform);
```

The macro specialises `ComponentTraits<T>` with:
- `IsRegistered = true` — required for compile-time checks
- `Name` — human-readable name for debugging and serialization
- `Id` — a monotonically increasing unique ID assigned by `ComponentIdGenerator`

```cpp
#define REGISTER_SD_COMPONENT(Type)                                          \
  template<>                                                                 \
  struct ComponentTraits<Type> {                                             \
    static constexpr bool IsRegistered = true;                               \
    static constexpr const char* Name = "SD_" #Type;                         \
    static inline const usize Id = SD::detail::ComponentIdGenerator::Next(); \
  };
```

**Two variants exist:**
- `REGISTER_SD_COMPONENT` — prepends `"SD_"` to the name (for engine-built components)
- `REGISTER_COMPONENT` — uses the raw type name as-is (for user-defined components)

**Serialization:** Components that need persistence specialise `ComponentSerializer<T>`. This is useful for saving/loading and also networking.

```cpp
template<>
struct ComponentSerializer<Transform> {
    static void Serialize(const Transform& c, Serializer& s) {
        s.Write(c.position);
        s.Write(c.rotation);
        s.Write(c.scale);
    }
    static void Deserialize(Transform& c, Serializer& s) {
        c.position = s.Read<Vec3>();
        c.rotation = s.Read<Quat>();
        c.scale = s.Read<Vec3>();
    }
};
```

> [!warning] Components
> Since rendering and physics, and more. Is not implemented yet, the components are very much WIP, and more components will be added, and existing components will be changed.
