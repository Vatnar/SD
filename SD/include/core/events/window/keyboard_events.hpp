#pragma once
#include "core/events/Event.hpp"

namespace sd {
inline auto key_category = EventCategory::EVENT_CATEGORY_WINDOW | EventCategory::EVENT_CATEGORY_INPUT | EventCategory::EVENT_CATEGORY_KEYBOARD;
class KeyPressedEvent : public Event {
public:
  int key{};
  int scancode{};
  int mods{};
  bool repeat{};
  KeyPressedEvent(int key, int scancode, int mods, bool repeat) :
    key(key), scancode(scancode), mods(mods), repeat(repeat) {}

  EVENT_CLASS_TYPE(KEY_PRESSED)
  EVENT_CLASS_CATEGORY(key_category)
};


class KeyReleasedEvent : public Event {
public:
  int key{};
  int scancode{};
  int mods{};

  KeyReleasedEvent(int key, int scancode, int mods) : key(key), scancode(scancode), mods(mods) {}

  EVENT_CLASS_TYPE(KEY_RELEASED)
  EVENT_CLASS_CATEGORY(key_category)
};

class KeyTypedEvent : public Event {
public:
  uint32_t keycode{};

  explicit KeyTypedEvent(uint32_t keycode) : keycode(keycode) {}

  EVENT_CLASS_TYPE(KEY_TYPED)
  EVENT_CLASS_CATEGORY(key_category)
};
} // namespace SD
