#pragma once
#include "SD/core/events/Event.hpp"

namespace sd {
class AppTerminateEvent : public Event {
public:
  AppTerminateEvent() = default;
  EVENT_CLASS_TYPE(APP_TERMINATE)
  EVENT_CLASS_CATEGORY(EventCategory::EVENT_CATEGORY_APPLICATION)
};
} // namespace sd
