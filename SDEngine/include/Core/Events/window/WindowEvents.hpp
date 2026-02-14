#pragma once
#include "Core/Events/Event.hpp"
class WindowResizeEvent : public Event {
public:
  int width{}, height{};
  WindowResizeEvent(int w, int h) : width(w), height(h) {}

  EVENT_CLASS_TYPE(WindowResize)
  EVENT_CLASS_CATEGORY(EventCategoryWindow)
};

class SwapchainOutOfDateEvent : public Event {
public:
  SwapchainOutOfDateEvent() = default;
  EVENT_CLASS_TYPE(SwapChainOutOfDate)
  EVENT_CLASS_CATEGORY(EventCategoryWindow)
};

class WindowCloseEvent : public Event {
public:
  WindowCloseEvent() = default;
  EVENT_CLASS_TYPE(WindowClose)
  EVENT_CLASS_CATEGORY(EventCategoryWindow)
};
