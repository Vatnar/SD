#pragma once

#include <algorithm>
#include <variant>

#include "EventVariant.hpp"
#include "SD/core/arena_vec.hpp"
#include "SD/export.hpp"

namespace sd {

struct SD_EXPORT EventManager {
  EventManager() {
    m_arena = arena_alloc(
        ArenaParams{.reserve_size = mb(1uz), .commit_size = kb(4uz), .name = "EventManagerArena"});
  }
  ~EventManager() {
    if (m_arena)
      arena_release(m_arena);
  }

  EventManager(const EventManager&)            = delete;
  EventManager& operator=(const EventManager&) = delete;
  EventManager(EventManager&&)                 = default;
  EventManager& operator=(EventManager&&)      = default;

  [[nodiscard]] bool has_resize_event() const {
    return std::ranges::any_of(m_events, [](const auto& e) {
      return std::holds_alternative<WindowResizeEvent>(e.event) ||
             std::holds_alternative<SwapchainOutOfDateEvent>(e.event);
    });
  }

  template<typename T>
  [[nodiscard]] bool has_event() const {
    return std::ranges::any_of(m_events,
                               [](const auto& e) { return std::holds_alternative<T>(e.event); });
  }

  template<typename T>
  void clear_type() {
    U64 write = 0;
    for (U64 i = 0; i < m_events.count; ++i) {
      if (!std::holds_alternative<T>(m_events.data[i].event)) {
        if (write != i)
          m_events.data[write] = m_events.data[i];
        ++write;
      }
    }
    m_events.count = write;
  }

  void clear() {
    m_events.clear();
    m_arena->clear();
  }

  template<typename T, typename... Args>
  void push_event(Args&&... args) {
    m_events.push(m_arena, EventVariant{.event = T(std::forward<Args>(args)...), .handled = false});
  }

  auto begin() { return m_events.begin(); }
  auto end() { return m_events.end(); }


  ArenaVec<EventVariant> m_events;
  Arena*                 m_arena = nullptr;
};

} // namespace sd
