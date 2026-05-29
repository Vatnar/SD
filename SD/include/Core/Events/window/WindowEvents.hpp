#pragma once
#include "Core/Events/Event.hpp"
namespace sd {
class WindowResizeEvent : public Event {
public:
  int width{}, height{};
  WindowResizeEvent(int w, int h) : width(w), height(h) {}

  EVENT_CLASS_TYPE(WINDOW_RESIZE)
  EVENT_CLASS_CATEGORY(EventCategory::EVENT_CATEGORY_WINDOW)
};

class SwapchainOutOfDateEvent : public Event {
public:
  SwapchainOutOfDateEvent() = default;
  EVENT_CLASS_TYPE(SWAPCHAIN_OUT_OF_DATE)
  EVENT_CLASS_CATEGORY(EventCategory::EVENT_CATEGORY_WINDOW)
};

class WindowCloseEvent : public Event {
public:
  WindowCloseEvent() = default;
  EVENT_CLASS_TYPE(WINDOW_CLOSE)
  EVENT_CLASS_CATEGORY(EventCategory::EVENT_CATEGORY_WINDOW)
};
} // namespace SD
