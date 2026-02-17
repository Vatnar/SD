#pragma once
#include "Core/Events/Event.hpp"

namespace SD {
static inline auto MouseCategory = EventCategoryWindow | EventCategoryInput | EventCategoryMouse;

class MousePressedEvent : public Event {
public:
  int button;
  int mods;
  bool repeat;

  explicit MousePressedEvent(int button, int mods, bool repeat) :
    button(button), mods(mods), repeat(repeat) {}

  EVENT_CLASS_TYPE(MousePressed)
  EVENT_CLASS_CATEGORY(MouseCategory | EventCategoryMouseButton)
};

class MouseReleasedEvent : public Event {
public:
  int button;
  int mods;

  explicit MouseReleasedEvent(int button, int mods) : button(button), mods(mods) {}
  EVENT_CLASS_TYPE(MouseReleased)
  EVENT_CLASS_CATEGORY(MouseCategory | EventCategoryMouseButton)
};

class MouseScrolledEvent : public Event {
public:
  double xOffset{}, yOffset{};
  MouseScrolledEvent(double xOffset, double yOffset) : xOffset(xOffset), yOffset(yOffset) {}

  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(MouseCategory)
};

class MouseMovedEvent : public Event {
public:
  double xPos{}, yPos{};
  MouseMovedEvent(double xPos, double yPos) : xPos(xPos), yPos(yPos) {}

  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(MouseCategory)
};
} // namespace SD
