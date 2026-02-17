#pragma once
#include "Core/Events/Event.hpp"

namespace SD {
class AppTerminateEvent : public Event {
public:
  AppTerminateEvent() = default;
  EVENT_CLASS_TYPE(AppTerminate)
  EVENT_CLASS_CATEGORY(EventCategoryApplication)
};
} // namespace SD
