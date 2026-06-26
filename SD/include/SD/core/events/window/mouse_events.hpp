#pragma once

namespace sd {

struct MousePressedEvent {
  int  button{};
  int  mods{};
  bool repeat{};
};

struct MouseReleasedEvent {
  int button{};
  int mods{};
};

struct MouseScrolledEvent {
  double x_offset{};
  double y_offset{};
};

struct MouseMovedEvent {
  double x_pos{};
  double y_pos{};
};

} // namespace sd
