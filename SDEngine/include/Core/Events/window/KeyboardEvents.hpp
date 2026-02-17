#pragma once
#include "Core/Events/Event.hpp"

namespace SD {
inline auto KeyCategory = EventCategoryWindow | EventCategoryInput | EventCategoryKeyboard;
class KeyPressedEvent : public Event {
public:
  int key{};
  int scancode{};
  int mods{};
  bool repeat{};
  KeyPressedEvent(int key, int mods, int scancode, bool repeat) :
    key(key), scancode(scancode), mods(mods), repeat(repeat) {}

  EVENT_CLASS_TYPE(KeyPressed)
  EVENT_CLASS_CATEGORY(KeyCategory)
};


class KeyReleasedEvent : public Event {
public:
  int key{};
  int scancode{};
  int mods{};

  KeyReleasedEvent(int key, int mods, int scancode) : key(key), scancode(scancode), mods(mods) {}

  EVENT_CLASS_TYPE(KeyReleased)
  EVENT_CLASS_CATEGORY(KeyCategory)
};

class KeyTypedEvent : public Event {
public:
  unsigned int keyCode{};

  explicit KeyTypedEvent(unsigned int keycode) : keyCode(keycode) {}

  EVENT_CLASS_TYPE(KeyTyped)
  EVENT_CLASS_CATEGORY(KeyCategory)
};
} // namespace SD
