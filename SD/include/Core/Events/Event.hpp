// TODO(docs): Add file-level Doxygen header
//   - @file Event.hpp
//   - @brief Event system base types and macros
//   - Event type/category system design
//   - Event macro usage (EVENT_CLASS_TYPE, EVENT_CLASS_CATEGORY)
#pragma once
#include <cstddef>
#include <string>
#include <type_traits>

#include "Core/types.hpp"

namespace sd {
// TODO(docs): Document BIT() helper
//   - Purpose: Compile-time bit shifting for event categories
static consteval usize bit(usize idx) {
  return 1ULL << idx;
}


// TODO(docs): Document EventType enum
//   - All event types and when they're fired
//   - Category relationships
enum class EventType {
  // clang-format off
  NONE = 0,
  /* Engine */
  /* Application*/ APP_TICK, APP_UPDATE, APP_RENDER, APP_TERMINATE,
  /* Window     */ WINDOW_CLOSE, WINDOW_RESIZE, WINDOW_FOCUS,
                   WINDOW_LOST_FOCUS, SWAPCHAIN_OUT_OF_DATE, // TODO: swapchain could just be windowresize
  /* Input      */
  /* Keyboard   */ KEY_PRESSED, KEY_RELEASED, KEY_TYPED,
  /* mouse      */ MOUSE_MOVED, MOUSE_SCROLLED,
  /* mousebutton*/ MOUSE_PRESSED, MOUSE_RELEASED,
  // clang-format on
};
enum class EventCategory : u16 {
  NONE = 0,
  EVENT_CATEGORY_ENGINE = bit(0),
  EVENT_CATEGORY_APPLICATION = bit(1), // For application specifics
  EVENT_CATEGORY_WINDOW = bit(2),
  EVENT_CATEGORY_INPUT = bit(3),
  EVENT_CATEGORY_KEYBOARD = bit(4),
  EVENT_CATEGORY_MOUSE = bit(5),
  EVENT_CATEGORY_MOUSE_BUTTON = bit(6),
  EVENT_CATEGORY_CONTROLLER = bit(7),
};
inline EventCategory operator|(const EventCategory a, const EventCategory b) {
  return static_cast<EventCategory>(static_cast<u16>(a) | static_cast<u16>(b));
}

inline bool operator&(int lhs, EventCategory rhs) {
  return (static_cast<u16>(lhs) & static_cast<u16>(rhs)) != 0;
}

class Event {
public:
  virtual ~Event() = default;
  virtual const char* get_name() const = 0;
  [[nodiscard]] virtual EventType get_event_type() const = 0;
  [[nodiscard]] virtual int get_category_flags() const = 0;

  [[nodiscard]] bool is_in_category(EventCategory category) const {
    return get_category_flags() & category;
  }

private:
  bool m_handled = false;

  friend class EventDispatcher;
  friend class LayerList;
};

#define EVENT_CLASS_TYPE(type)                       \
  static EventType get_static_type() {               \
    return EventType::type;                          \
  }                                                  \
  virtual EventType get_event_type() const override { \
    return get_static_type();                        \
  }                                                  \
  virtual const char* get_name() const override {    \
    return #type;                                    \
  }
#define EVENT_CLASS_CATEGORY(category)            \
  virtual int get_category_flags() const override { \
    return static_cast<int>(category);              \
  }
} // namespace SD
