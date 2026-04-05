#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Core/types.hpp>
#include "Utils/Serialization.hpp"

namespace SD {

class Command;
class CommandQueue;
class EntityManager;

class CommandRegistry {
public:
  static u32 Register(const char* name, u32 componentId = 0);
  static u32 GetId(const char* name, u32 componentId = 0);
  static const char* GetName(u32 id);

private:
  static inline std::vector<const char*> names;
  static inline std::unordered_map<std::string, u32> map;
};

class CommandFactory {
public:
  using CreatorFunc = std::function<std::unique_ptr<Command>()>;

  static void Register(u32 typeId, CreatorFunc creator);
  static std::unique_ptr<Command> Create(u32 typeId);

private:
  static inline std::vector<CreatorFunc> creators;
};

#define COMMAND_ID(Name)                                                  \
public:                                                                    \
  static u32 sTypeId;                                                    \
  u32 GetTypeId() const override { return sTypeId; }                    \
private:

#define COMMAND_ID_T(Name, T)                                             \
public:                                                                    \
  static u32 sTypeId;                                                    \
  u32 GetTypeId() const override { return sTypeId; }                    \
private:

#define REGISTER_COMMAND_FACTORY(Name)                                    \
  namespace {                                                             \
    static SD::CommandFactory::CreatorFunc Name##Creator =               \
      [] { return std::make_unique<Name>(); };                            \
    struct Name##AutoRegister {                                           \
      Name##AutoRegister() {                                             \
        SD::CommandFactory::Register(Name::sTypeId, Name##Creator);     \
      }                                                                   \
    };                                                                    \
    static Name##AutoRegister Name##Registrar;                           \
  }

/**
 * Represents a "future" entity in the command queue
 */
struct EntityHandle {
  u32 id;
  EntityHandle() : id(type_max<u32>) {}
  explicit EntityHandle(const u32 id) : id(id) {}

  [[nodiscard]] bool IsValid() const { return id != type_max<u32>; }

  bool operator==(const EntityHandle& other) const { return id == other.id; }
};

class Command {
public:
  virtual ~Command() = default;

  virtual u32 GetTypeId() const = 0;

  /**
   * Called when Apply() is called on the queue
   * @param em entity manager
   */
  virtual void Execute(EntityManager& em, CommandQueue& queue) = 0;

  /**
   * Serialize command to buffer (networking and files)
   * @param serializer serializer to write to
   */
  virtual void Serialize(Serializer& serializer) const = 0;

  /**
   * Deserialize command from buffer
   * @param serializer serializer to read from
   */
  virtual void Deserialize(Serializer& serializer) = 0;
};
} // namespace SD
