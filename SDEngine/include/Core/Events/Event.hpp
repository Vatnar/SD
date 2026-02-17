#pragma once
#include <cstddef>

#include "Core/types.hpp"

namespace SD {
static consteval usize BIT(usize idx) {
  return 1ULL << idx;
}


enum class EventType {
  // clang-format off
  None = 0,
  /* Engine */
  /* Application*/ AppTick, AppUpdate, AppRender, AppTerminate,
  /* Window     */ WindowClose, WindowResize, WindowFocus,
                   WindowLostFocus, SwapChainOutOfDate, // TODO: swapchain could just be windowresize
  /* Input      */
  /* Keyboard   */ KeyPressed, KeyReleased, KeyTyped,
  /* mouse      */ MouseMoved, MouseScrolled,
  /* mousebutton*/ MousePressed, MouseReleased,
  // clang-format on
};
enum EventCategory : u16 {
  None = 0,
  EventCategoryEngine = BIT(0),
  EventCategoryApplication = BIT(1), // For application specifics
  EventCategoryWindow = BIT(2),
  EventCategoryInput = BIT(3),
  EventCategoryKeyboard = BIT(4),
  EventCategoryMouse = BIT(5),
  EventCategoryMouseButton = BIT(6),
  EventCategoryController = BIT(7),
};
inline EventCategory operator|(const EventCategory a, const EventCategory b) {
  return static_cast<EventCategory>(static_cast<u16>(a) | static_cast<u16>(b));
}

class Event {
public:
  virtual ~Event() = default;
  [[nodiscard]] virtual EventType GetEventType() const = 0;
  [[nodiscard]] virtual int GetCategoryFlags() const = 0;

  [[nodiscard]] bool IsInCategory(EventCategory category) const {
    return GetCategoryFlags() & category;
  }
  [[nodiscard]] virtual const char* GetName() const = 0;

  bool isHandled = false;
};

#define EVENT_CLASS_TYPE(type)                      \
  static EventType GetStaticType() {                \
    return EventType::type;                         \
  }                                                 \
  virtual EventType GetEventType() const override { \
    return GetStaticType();                         \
  }                                                 \
  virtual const char* GetName() const override {    \
    return #type;                                   \
  }
#define EVENT_CLASS_CATEGORY(category)            \
  virtual int GetCategoryFlags() const override { \
    return category;                              \
  }
} // namespace SD
