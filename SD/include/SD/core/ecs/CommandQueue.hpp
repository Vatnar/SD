#pragma once

#include <mutex>
#include <unordered_map>

#include "Command.hpp"
#include "Entity.hpp"
#include "SD/core/arena_vec.hpp"

namespace sd {

struct TypeErasedCommandEntry {
  void* (*alloc_fn)(Arena* arena);
  void (*execute_fn)(void* data, EntityManager<ComponentGroup<>>& em, CommandQueue& queue);
  void (*serialize_fn)(void* data, Serializer& s);
  void (*deserialize_fn)(void* data, Serializer& s);
};

struct CommandQueue {
  CommandQueue() : m_arena(arena_alloc(ArenaParams{.name = "CommandQueueArena"})) {}
  ~CommandQueue() {
    if (m_arena)
      arena_release(m_arena);
  }

  CommandQueue(const CommandQueue&)            = delete;
  CommandQueue& operator=(const CommandQueue&) = delete;
  CommandQueue(CommandQueue&&)                 = delete;
  CommandQueue& operator=(CommandQueue&&)      = delete;

  template<typename T, typename... Args>
  void add(Args&&... args) {
    T* data = m_arena->push_array<T>(1);
    *data   = T(std::forward<Args>(args)...);
    m_commands.push(
        m_arena,
        CommandNode{
            .type_id        = type_id_of<T>(),
            .data           = data,
            .execute_fn     = [](void*                            d,
                                 EntityManager<ComponentGroup<>>& em,
                                 CommandQueue& queue) { static_cast<T*>(d)->execute(em, queue); },
            .serialize_fn   = [](void* d, Serializer& s) { static_cast<T*>(d)->serialize(s); },
            .deserialize_fn = [](void* d, Serializer& s) { static_cast<T*>(d)->deserialize(s); },
        });
  }

  void apply(EntityManager<ComponentGroup<>>& em);

  [[nodiscard]] Entity get_entity(EntityHandle handle) const;
  void                 set_entity_for_handle(EntityHandle entity_handle, Entity entity);
  [[nodiscard]] bool   is_handle_resolved(EntityHandle handle) const;

  void                clear();
  [[nodiscard]] USize get_count() const;

  void serialize(Serializer& serializer) const;
  void deserialize(Serializer& serializer);

  [[nodiscard]] Arena* arena() const { return m_arena; }

  // Deserialization registry
  static void register_type_erased_entry(U64 type_id, TypeErasedCommandEntry entry);
  static TypeErasedCommandEntry lookup_type_erased_entry(U64 type_id);


  ArenaVec<CommandNode> m_commands;
  ArenaVec<Entity>      m_handle_to_entity;
  std::mutex            m_mutex;
  Arena*                m_arena;

  static inline std::unordered_map<U64, TypeErasedCommandEntry> s_type_entries;
};

} // namespace sd
