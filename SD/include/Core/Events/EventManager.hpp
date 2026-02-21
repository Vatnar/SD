#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "Event.hpp"

namespace SD {
class EventManager {
public:
  EventManager() { mEvents.reserve(64); }

  EventManager(const EventManager&) = delete;
  EventManager& operator=(const EventManager&) = delete;
  EventManager(EventManager&&) = default;
  EventManager& operator=(EventManager&&) = default;

  [[nodiscard]] bool HasResizeEvent() const {
    return std::ranges::any_of(mEvents, [](const auto& e) {
      return e->GetEventType() == EventType::WindowResize ||
             e->GetEventType() == EventType::SwapChainOutOfDate;
    });
  }

  template<typename T>
    requires std::derived_from<T, Event>
  [[nodiscard]] bool HasEvent() const {
    return std::ranges::any_of(
        mEvents, [](const auto& e) { return e->GetEventType() == T::GetStaticType(); });
  }


  void Clear() { mEvents.clear(); }


  template<typename T, typename... Args>
    requires std::derived_from<T, Event>
  void PushEvent(Args&&... args) {
    mEvents.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

  template<typename T>
    requires std::derived_from<T, Event>
  void ClearType() {
    auto [new_end, _] = std::ranges::remove_if(
        mEvents, [](const auto& e) { return e->GetEventType() == T::GetStaticType(); });

    mEvents.erase(new_end, mEvents.end());
  }


  auto begin() { return mEvents.begin(); }
  auto end() { return mEvents.end(); }
  [[nodiscard]] auto begin() const { return mEvents.begin(); }
  [[nodiscard]] auto end() const { return mEvents.end(); }

private:
  std::vector<std::unique_ptr<Event>> mEvents;
};

class EventDispatcher {
public:
  explicit EventDispatcher(Event& event) : mEvent(event) {}

  template<typename T, typename F>
  bool Dispatch(const F& func) {
    if (mEvent.GetEventType() == T::GetStaticType()) {
      mEvent.isHandled |= func(static_cast<T&>(mEvent));
      return true;
    }
    return false;
  }

private:
  Event& mEvent;
};
} // namespace SD
