#include "SD/core/ecs/CommandQueue.hpp"

#include "SD/core/ecs/commands.hpp"
#include "SD/utils/utils.hpp"

namespace sd {

FILE_INTERNAL_BEGIN
struct CommandTypeRegistrar {
  CommandTypeRegistrar() {
    CommandQueue::register_type_erased_entry(
        type_id_of<CreateEntityCmd>(),
        TypeErasedCommandEntry{
            .alloc_fn = [](Arena* a) -> void* { return a->push_array<CreateEntityCmd>(1); },
            .execute_fn =
                [](void* d, EntityManager<ComponentGroup<>>& em, CommandQueue& queue) {
                  static_cast<CreateEntityCmd*>(d)->execute(em, queue);
                },
            .serialize_fn = [](void*       d,
                               Serializer& s) { static_cast<CreateEntityCmd*>(d)->serialize(s); },
            .deserialize_fn =
                [](void* d, Serializer& s) { static_cast<CreateEntityCmd*>(d)->deserialize(s); },
        });
    CommandQueue::register_type_erased_entry(
        type_id_of<DestroyEntityCmd>(),
        TypeErasedCommandEntry{
            .alloc_fn = [](Arena* a) -> void* { return a->push_array<DestroyEntityCmd>(1); },
            .execute_fn =
                [](void* d, EntityManager<ComponentGroup<>>& em, CommandQueue& queue) {
                  static_cast<DestroyEntityCmd*>(d)->execute(em, queue);
                },
            .serialize_fn = [](void*       d,
                               Serializer& s) { static_cast<DestroyEntityCmd*>(d)->serialize(s); },
            .deserialize_fn =
                [](void* d, Serializer& s) { static_cast<DestroyEntityCmd*>(d)->deserialize(s); },
        });
  }
};
CommandTypeRegistrar s_registrar;

FILE_INTERNAL_END

void CommandQueue::apply(EntityManager<ComponentGroup<>>& em) {
  for (U64 i = 0; i < m_commands.count; ++i) {
    auto& cmd = m_commands.data[i];
    cmd.execute_fn(cmd.data, em, *this);
  }
  clear();
}

void CommandQueue::set_entity_for_handle(EntityHandle entity_handle, Entity entity) {
  ASSERT(entity_handle.is_valid() && "Cannot set entity for invalid (sentinel) handle");
  if (entity_handle.id >= m_handle_to_entity.count) {
    // Grow the handle table — push placeholder entities
    for (U64 i = m_handle_to_entity.count; i <= entity_handle.id; ++i)
      m_handle_to_entity.push(m_arena, Entity{});
    m_handle_to_entity.data[entity_handle.id] = entity;
  } else {
    m_handle_to_entity.data[entity_handle.id] = entity;
  }
}

bool CommandQueue::is_handle_resolved(EntityHandle handle) const {
  return handle.is_valid() && handle.id < m_handle_to_entity.count;
}

void CommandQueue::clear() {
  m_commands.clear();
  m_handle_to_entity.clear();
  m_arena->clear();
}

USize CommandQueue::get_count() const {
  return m_commands.count;
}

Entity CommandQueue::get_entity(EntityHandle handle) const {
  ASSERT(handle.id < m_handle_to_entity.count && "Entity has not been resolved");
  return m_handle_to_entity.data[handle.id];
}

void CommandQueue::register_type_erased_entry(U64 type_id, TypeErasedCommandEntry entry) {
  s_type_entries[type_id] = entry;
}

TypeErasedCommandEntry CommandQueue::lookup_type_erased_entry(U64 type_id) {
  auto it = s_type_entries.find(type_id);
  if (it != s_type_entries.end())
    return it->second;
  return {};
}

void CommandQueue::serialize(Serializer& serializer) const {
  serializer.write(static_cast<U32>(m_commands.count));
  for (U64 i = 0; i < m_commands.count; ++i) {
    auto& cmd = m_commands.data[i];
    serializer.write(static_cast<U32>(cmd.type_id));
    std::vector<std::byte> payload;
    Serializer             payload_serializer(payload);
    cmd.serialize_fn(cmd.data, payload_serializer);
    serializer.write(static_cast<U32>(payload.size()));
    serializer.write(payload.data(), payload.size());
  }
}

void CommandQueue::deserialize(Serializer& serializer) {
  U32 count = serializer.read<U32>();
  clear();

  for (U32 i = 0; i < count; ++i) {
    U32   type_id       = serializer.read<U32>();
    U32   payload_size  = serializer.read<U32>();
    USize payload_start = serializer.get_offset();

    auto entry = lookup_type_erased_entry(type_id);
    if (entry.deserialize_fn) {
      void* data = entry.alloc_fn(m_arena);
      entry.deserialize_fn(data, serializer);
      m_commands.push(m_arena,
                      CommandNode{
                          .type_id        = type_id,
                          .data           = data,
                          .execute_fn     = entry.execute_fn,
                          .serialize_fn   = entry.serialize_fn,
                          .deserialize_fn = entry.deserialize_fn,
                      });
    } else {
      log::engine::error("Unknown command type ID {} during deserialization, skipping {} bytes",
                         type_id,
                         payload_size);
      serializer.SetOffset(payload_start + payload_size);
    }
  }
}

} // namespace sd
