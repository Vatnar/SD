#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "EntityManager.hpp"
#include "SD/core/types.hpp"
#include "SD/utils/serialization.hpp"

namespace sd {

class Command;
class CommandQueue;

class CommandRegistry {
public:
  static U32         register_(const char* name, U32 component_id = 0);
  static U32         get_id(const char* name, U32 componentId = 0);
  static const char* get_name(U32 id);

private:
  static inline std::vector<const char*>             names;
  static inline std::unordered_map<std::string, U32> map;
};

class CommandFactory {
public:
  using CreatorFn = std::function<std::unique_ptr<Command>()>;

  static void                     register_(U32 type_id, CreatorFn creator);
  static std::unique_ptr<Command> create(U32 type_id);

private:
  static inline std::vector<CreatorFn> creators;
};

#define COMMAND_ID(Name)                           \
public:                                            \
  inline static U32 s_type_id;                     \
  U32               get_type_id() const override { \
    return s_type_id;                              \
  }                                                \
                                                   \
private:

#define COMMAND_ID_T(Name, T)                      \
public:                                            \
  inline static U32 s_type_id;                     \
  U32               get_type_id() const override { \
    return s_type_id;                              \
  }                                                \
                                                   \
private:

#define REGISTER_COMMAND(Name)                                                    \
  namespace {                                                                     \
  struct Name##_CommandRegistrar {                                                \
    Name##_CommandRegistrar() {                                                   \
      sd::Name::s_type_id = sd::CommandRegistry::register_(#Name);                \
      sd::CommandFactory::register_(sd::Name::s_type_id,                          \
                                    [] { return std::make_unique<sd::Name>(); }); \
    }                                                                             \
  };                                                                              \
  static Name##_CommandRegistrar s_##Name##_registrar;                            \
  }

#define REGISTER_COMPONENT_COMMANDS(T)                                                             \
  namespace {                                                                                      \
  struct AddComponentCmd##T##_Registrar {                                                          \
    AddComponentCmd##T##_Registrar() {                                                             \
      sd::AddComponentCmd<T>::s_type_id =                                                          \
          sd::CommandRegistry::register_("AddComponentCmd", sd::ComponentTraits<T>::id);           \
      sd::CommandFactory::register_(sd::AddComponentCmd<T>::s_type_id,                             \
                                    [] { return std::make_unique<sd::AddComponentCmd<T>>(); });    \
    }                                                                                              \
  };                                                                                               \
  static AddComponentCmd##T##_Registrar s_AddComponentCmd##T##_registrar;                          \
  }                                                                                                \
  namespace {                                                                                      \
  struct RemoveComponentCmd##T##_Registrar {                                                       \
    RemoveComponentCmd##T##_Registrar() {                                                          \
      sd::RemoveComponentCmd<T>::s_type_id =                                                       \
          sd::CommandRegistry::register_("RemoveComponentCmd", sd::ComponentTraits<T>::id);        \
      sd::CommandFactory::register_(sd::RemoveComponentCmd<T>::s_type_id,                          \
                                    [] { return std::make_unique<sd::RemoveComponentCmd<T>>(); }); \
    }                                                                                              \
  };                                                                                               \
  static RemoveComponentCmd##T##_Registrar s_RemoveComponentCmd##T##_registrar;                    \
  }

/**
 * Represents a "future" entity in the command queue
 */
struct EntityHandle {
  U32 id;
  EntityHandle() : id(g_type_max<U32>) {}
  explicit EntityHandle(const U32 id) : id(id) {}

  [[nodiscard]] bool is_valid() const { return id != g_type_max<U32>; }

  bool operator==(const EntityHandle& other) const { return id == other.id; }
  bool operator!=(const EntityHandle& other) const { return !(*this == other); }
};

class Command {
public:
  virtual ~Command() = default;

  virtual U32 get_type_id() const = 0;

  /**
   * Called when Apply() is called on the queue
   * @param em entity manager
   */
  virtual void execute(EntityManager<ComponentGroup<>>& em, CommandQueue& queue) = 0;

  /**
   * Serialize command to buffer (networking and files)
   * @param serializer serializer to write to
   */
  virtual void serialize(Serializer& serializer) const = 0;

  /**
   * Deserialize command from buffer
   * @param serializer serializer to read from
   */
  virtual void deserialize(Serializer& serializer) = 0;
};
} // namespace sd

template<>
struct std::hash<sd::EntityHandle> {
  inline std::size_t operator()(sd::EntityHandle h) const noexcept {
    return std::hash<U32>{}(h.id);
  }
};
