#pragma once

namespace sd {

struct WindowResizeEvent {
  int width{};
  int height{};
};

struct SwapchainOutOfDateEvent {};

struct WindowCloseEvent {};

} // namespace sd
