#pragma once
#include "Command.hpp"
#include "Entity.hpp"

namespace sd {

class CommandQueue {
public:
  template<typename T, typename... Args>
  void add(Args&&... args) {
    m_commands.push_back(std::make_unique<T>(std::forward<Args>(args)...));
  }
  void apply(EntityManager& em);

  [[nodiscard]] Entity get_entity(EntityHandle handle) const;

  void set_entity_for_handle(EntityHandle entity_handle, Entity entity);
  [[nodiscard]] bool is_handle_resolved(EntityHandle handle) const;
  void clear();
  [[nodiscard]] usize get_count() const;

  void serialize(Serializer& serializer) const;
  void deserialize(Serializer& serializer);

private:
  std::vector<std::unique_ptr<Command>> m_commands;
  std::vector<Entity> m_handle_to_entity;
  std::mutex m_mutex;
};

} // namespace SD
