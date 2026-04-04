#pragma once

#include <memory>
#include <vector>

#include "Core/types.hpp"
#include "Utils/Serialization.hpp"

namespace SD {
class CommandQueue;
class EntityManager;

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
