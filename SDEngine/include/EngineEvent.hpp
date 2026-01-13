#pragma once

enum class EngineEventCategory
{
    WindowResize,
    SwapchainOutOfDate
};

struct EngineEvent
{
    bool Handled{false};
    virtual ~EngineEvent()                                     = default;
    [[nodiscard]] virtual EngineEventCategory category() const = 0;
};

struct WindowResizeEvent : EngineEvent
{
    int                               width{}, height{};
    [[nodiscard]] EngineEventCategory category() const override { return EngineEventCategory::WindowResize; }
    WindowResizeEvent() = default;
    WindowResizeEvent(int w, int h) : width(w), height(h) {}
};
struct SwapchainOutOfDateEvent : EngineEvent
{
    [[nodiscard]] EngineEventCategory category() const override { return EngineEventCategory::SwapchainOutOfDate; }
    SwapchainOutOfDateEvent() = default;
};
