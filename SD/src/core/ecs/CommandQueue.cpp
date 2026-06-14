#include "SD/core/ecs/CommandQueue.hpp"

#include "SD/core/ecs/commands.hpp"
#include "SD/utils/utils.hpp"

namespace sd {

U32 CommandRegistry::register_(const char* name, U32 component_id) {
  std::string key = std::string(name) + "_" + std::to_string(component_id);
  auto        it  = map.find(key);
  if (it != map.end()) {
    return it->second;
  }
  U32 id = static_cast<U32>(names.size());
  names.push_back(name);
  map[key] = id;
  return id;
}

U32 CommandRegistry::get_id(const char* name, U32 component_id) {
  std::string key = std::string(name) + "_" + std::to_string(component_id);
  auto        it  = map.find(key);
  if (it != map.end()) {
    return it->second;
  }
  return g_type_max<U32>;
}

const char* CommandRegistry::get_name(U32 id) {
  if (id < names.size()) {
    return names[id];
  }
  return "Unknown";
}

void CommandFactory::register_(U32 type_id, CreatorFn creator) {
  if (type_id >= creators.size()) {
    creators.resize(type_id + 1);
  }
  creators[type_id] = std::move(creator);
}

std::unique_ptr<Command> CommandFactory::create(U32 type_id) {
  if (type_id < creators.size() && creators[type_id]) {
    return creators[type_id]();
  }
  return nullptr;
}

void CommandQueue::apply(EntityManager<ComponentGroup<>>& em) {
  for (const auto& cmd : m_commands) {
    cmd->execute(em, *this);
  }
  m_commands.clear();
}
void CommandQueue::set_entity_for_handle(EntityHandle entity_handle, Entity entity) {
  ASSERT(entity_handle.is_valid() && "Cannot set entity for invalid (sentinel) handle");
  if (entity_handle.id >= m_handle_to_entity.size()) {
    m_handle_to_entity.resize(entity_handle.id + 1);
  }
  m_handle_to_entity[entity_handle.id] = entity;
}
bool CommandQueue::is_handle_resolved(EntityHandle handle) const {
  return handle.is_valid() && handle.id < m_handle_to_entity.size();
}
void CommandQueue::clear() {
  m_commands.clear();
  m_handle_to_entity.clear();
}
USize CommandQueue::get_count() const {
  return m_commands.size();
}
Entity CommandQueue::get_entity(EntityHandle handle) const {
  ASSERT(handle.id < m_handle_to_entity.size() && "Entity has not been resolved");
  return m_handle_to_entity[handle.id];
}

void CommandQueue::serialize(Serializer& serializer) const {
  serializer.write(static_cast<U32>(m_commands.size()));
  for (const auto& cmd : m_commands) {
    serializer.write(cmd->get_type_id());
    // Serialize command to temp buffer to know its size
    std::vector<std::byte> payload;
    Serializer             payload_serializer(payload);
    cmd->serialize(payload_serializer);
    serializer.write(static_cast<U32>(payload.size()));
    serializer.write(payload.data(), payload.size());
  }
}

void CommandQueue::deserialize(Serializer& serializer) {
  U32 count = serializer.read<U32>();
  m_commands.clear();
  m_commands.reserve(count);

  for (U32 i = 0; i < count; ++i) {
    U32   type_id       = serializer.read<U32>();
    U32   payload_size  = serializer.read<U32>();
    USize payload_start = serializer.get_offset();
    if (auto cmd = CommandFactory::create(type_id)) {
      cmd->deserialize(serializer);
      m_commands.push_back(std::move(cmd));
    } else {
      log::engine::error("Unknown command type ID {} during deserialization, skipping {} bytes",
                         type_id,
                         payload_size);
      serializer.SetOffset(payload_start + payload_size);
    }
  }
}

} // namespace sd
