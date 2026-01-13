#pragma once

#include <vector>
#include <memory>
#include <algorithm>

#include "EngineEvent.hpp"
#include "InputEvent.hpp"

using InputEventManager = std::vector<std::unique_ptr<InputEvent>>;

class EngineEventManager
{
public:
    // TODO: prealloc some prolly
    EngineEventManager()                                     = default;
    EngineEventManager(const EngineEventManager&)            = delete;
    EngineEventManager& operator=(const EngineEventManager&) = delete;

    EngineEventManager(EngineEventManager&&)            = default;
    EngineEventManager& operator=(EngineEventManager&&) = default;

    [[nodiscard]] bool HasResizeEvent() const
    {
        return std::ranges::any_of(events,
                                   [](const auto& e)
                                   {
                                       return e->category() == EngineEventCategory::WindowResize ||
                                              e->category() == EngineEventCategory::SwapchainOutOfDate;
                                   });
    }
    void Clear() { events.clear(); }


    template<typename T, typename... Args>
        requires std::derived_from<T, EngineEvent>
    void PushEvent(Args&&...args)
    {
        events.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    }


    template<typename T>
        requires std::derived_from<T, EngineEvent>
    void ClearType()
    {
        auto new_end = std::ranges::remove_if(events, [](const auto& e) { return e->category() == T{}.category(); });

        events.erase(new_end.begin(), new_end.end());
    }


private:
    std::vector<std::unique_ptr<EngineEvent>> events;
};
