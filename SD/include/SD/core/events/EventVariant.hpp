#pragma once

#include <variant>

#include "SD/core/events/app/app_events.hpp"
#include "SD/core/events/window/keyboard_events.hpp"
#include "SD/core/events/window/mouse_events.hpp"
#include "SD/core/events/window/window_events.hpp"

namespace sd {

template<typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct EventVariant {
  std::variant<
      // Keyboard
      KeyPressedEvent,
      KeyReleasedEvent,
      KeyTypedEvent,
      // Mouse
      MousePressedEvent,
      MouseReleasedEvent,
      MouseMovedEvent,
      MouseScrolledEvent,
      // Window
      WindowResizeEvent,
      WindowCloseEvent,
      SwapchainOutOfDateEvent,
      // App
      AppTerminateEvent>
      event;

  bool handled = false;
};

} // namespace sd
