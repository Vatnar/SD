// TODO(docs): Add file-level Doxygen header
//   - @file EventManager.hpp
//   - @brief Event queue and dispatcher
//   - Event queuing vs immediate dispatch patterns
#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "Event.hpp"

namespace sd {
// TODO(docs): Document EventManager class
//   - Purpose: Queued event storage and type queries
//   - Event lifetime (pushed, queried, cleared)
//   - Integration with LayerList event handling
//   - Example: Checking for specific events
class EventManager {
public:
  EventManager() { m_events.reserve(64); }

  EventManager(const EventManager&) = delete;
  EventManager& operator=(const EventManager&) = delete;
  EventManager(EventManager&&) = default;
  EventManager& operator=(EventManager&&) = default;

  [[nodiscard]] bool has_resize_event() const {
    return std::ranges::any_of(m_events, [](const auto& e) {
      return e->get_event_type() == EventType::WINDOW_RESIZE ||
             e->get_event_type() == EventType::SWAPCHAIN_OUT_OF_DATE;
    });
  }

  template<typename T>
    requires std::derived_from<T, Event>
  [[nodiscard]] bool has_event() const {
    return std::ranges::any_of(
        m_events, [](const auto& e) { return e->get_event_type() == T::get_static_type(); });
  }


  void clear() { m_events.clear(); }


  template<typename T, typename... Args>
    requires std::derived_from<T, Event>
  void push_event(Args&&... args) {
    m_events.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

  template<typename T>
    requires std::derived_from<T, Event>
  void clear_type() {
    auto ret = std::ranges::remove_if(
        m_events, [](const auto& e) { return e->get_event_type() == T::GetStaticType(); });

    m_events.erase(ret.begin(), m_events.end());
  }


  auto begin() { return m_events.begin(); }
  auto end() { return m_events.end(); }
  [[nodiscard]] auto begin() const { return m_events.begin(); }
  [[nodiscard]] auto end() const { return m_events.end(); }

private:
  std::vector<std::unique_ptr<Event>> m_events;
};

class EventDispatcher {
public:
  explicit EventDispatcher(Event& event) : m_event(event) {}

  template<typename T, typename F>
  bool dispatch(const F& func) {
    if (m_event.get_event_type() == T::get_static_type()) {
      if (func(static_cast<T&>(m_event))) {
        m_event.m_handled = true;
      }
      return true;
    }
    return false;
  }

private:
  Event& m_event;
};
} // namespace SD
