#pragma once
#include "Core/Events/Event.hpp"

namespace sd {
static inline auto g_mouse_category = EventCategory::EVENT_CATEGORY_WINDOW | EventCategory::EVENT_CATEGORY_INPUT | EventCategory::EVENT_CATEGORY_MOUSE;

class MousePressedEvent : public Event {
public:
  int button;
  int mods;
  bool repeat;

  explicit MousePressedEvent(int button, int mods, bool repeat) :
    button(button), mods(mods), repeat(repeat) {}

  EVENT_CLASS_TYPE(MOUSE_PRESSED)
  EVENT_CLASS_CATEGORY(g_mouse_category | EventCategory::EVENT_CATEGORY_MOUSE_BUTTON)
};

class MouseReleasedEvent : public Event {
public:
  int button;
  int mods;

  explicit MouseReleasedEvent(int button, int mods) : button(button), mods(mods) {}
  EVENT_CLASS_TYPE(MOUSE_RELEASED)
  EVENT_CLASS_CATEGORY(g_mouse_category | EventCategory::EVENT_CATEGORY_MOUSE_BUTTON)
};

class MouseScrolledEvent : public Event {
public:
  double x_offset{}, y_offset{};
  MouseScrolledEvent(double x_offset, double y_offset) : x_offset(x_offset), y_offset(y_offset) {}

  EVENT_CLASS_TYPE(MOUSE_SCROLLED)
  EVENT_CLASS_CATEGORY(g_mouse_category)
};

class MouseMovedEvent : public Event {
public:
  double x_pos{}, y_pos{};
  MouseMovedEvent(double x_pos, double y_pos) : x_pos(x_pos), y_pos(y_pos) {}

  EVENT_CLASS_TYPE(MOUSE_MOVED)
  EVENT_CLASS_CATEGORY(g_mouse_category)
};
} // namespace SD
