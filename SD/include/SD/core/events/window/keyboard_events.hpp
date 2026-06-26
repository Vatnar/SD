#pragma once

namespace sd {

struct KeyPressedEvent {
  int  key{};
  int  scancode{};
  int  mods{};
  bool repeat{};
};

struct KeyReleasedEvent {
  int key{};
  int scancode{};
  int mods{};
};

struct KeyTypedEvent {
  uint32_t keycode{};
};

} // namespace sd
