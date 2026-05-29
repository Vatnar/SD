#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Core/types.hpp>
#include "Utils/Serialization.hpp"

namespace sd {

class Command;
class CommandQueue;
class EntityManager;

class CommandRegistry {
public:
  static u32 register_(const char* name, u32 component_id = 0);
  static u32 get_id(const char* name, u32 componentId = 0);
  static const char* get_name(u32 id);

private:
  static inline std::vector<const char*> names;
  static inline std::unordered_map<std::string, u32> map;
};

class CommandFactory {
public:
  using CreatorFn = std::function<std::unique_ptr<Command>()>;

  static void register_(u32 type_id, CreatorFn creator);
  static std::unique_ptr<Command> create(u32 type_id);

private:
  static inline std::vector<CreatorFn> creators;
};

#define COMMAND_ID(Name)                                                   \
public:                                                                     \
  inline static u32 s_type_id;                                              \
  u32 get_type_id() const override { return s_type_id; }                    \
private:

#define COMMAND_ID_T(Name, T)                                              \
public:                                                                     \
  inline static u32 s_type_id;                                              \
  u32 get_type_id() const override { return s_type_id; }                    \
private:

#define REGISTER_COMMAND(Name)                                             \
  namespace {                                                               \
    struct Name##_CommandRegistrar {                                        \
      Name##_CommandRegistrar() {                                             \
        sd::Name::s_type_id = sd::CommandRegistry::register_(#Name);        \
        sd::CommandFactory::register_(sd::Name::s_type_id,                   \
          [] { return std::make_unique<sd::Name>(); });                     \
      }                                                                     \
    };                                                                      \
    static Name##_CommandRegistrar s_##Name##_registrar;                    \
  }

#define REGISTER_COMPONENT_COMMANDS(T)                                     \
  namespace {                                                               \
    struct AddComponentCmd##T##_Registrar {                                 \
      AddComponentCmd##T##_Registrar() {                                    \
        sd::AddComponentCmd<T>::s_type_id =                                \
          sd::CommandRegistry::register_("AddComponentCmd",                \
                                         sd::ComponentTraits<T>::id);      \
        sd::CommandFactory::register_(sd::AddComponentCmd<T>::s_type_id,   \
          [] { return std::make_unique<sd::AddComponentCmd<T>>(); });       \
      }                                                                     \
    };                                                                      \
    static AddComponentCmd##T##_Registrar s_AddComponentCmd##T##_registrar; \
  }                                                                         \
  namespace {                                                               \
    struct RemoveComponentCmd##T##_Registrar {                              \
      RemoveComponentCmd##T##_Registrar() {                                 \
        sd::RemoveComponentCmd<T>::s_type_id =                             \
          sd::CommandRegistry::register_("RemoveComponentCmd",              \
                                         sd::ComponentTraits<T>::id);        \
        sd::CommandFactory::register_(sd::RemoveComponentCmd<T>::s_type_id,  \
          [] { return std::make_unique<sd::RemoveComponentCmd<T>>(); });    \
      }                                                                     \
    };                                                                      \
    static RemoveComponentCmd##T##_Registrar s_RemoveComponentCmd##T##_registrar; \
  }

/**
 * Represents a "future" entity in the command queue
 */
struct EntityHandle {
  u32 id;
  EntityHandle() : id(g_type_max<u32>) {}
  explicit EntityHandle(const u32 id) : id(id) {}

  [[nodiscard]] bool is_valid() const { return id != g_type_max<u32>; }

  bool operator==(const EntityHandle& other) const { return id == other.id; }
  bool operator!=(const EntityHandle& other) const { return !(*this == other); }
};

class Command {
public:
  virtual ~Command() = default;

  virtual u32 get_type_id() const = 0;

  /**
   * Called when Apply() is called on the queue
   * @param em entity manager
   */
  virtual void execute(EntityManager& em, CommandQueue& queue) = 0;

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
} // namespace SD

template<>
struct std::hash<sd::EntityHandle> {
  inline std::size_t operator()(sd::EntityHandle h) const noexcept {
    return std::hash<u32>{}(h.id);
  }
};
