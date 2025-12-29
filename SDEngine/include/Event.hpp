#pragma once
#include <cstdint>

enum class EventCategory : uint32_t
{
    Window = 1 << 0,
    Mouse  = 1 << 1,
    Key    = 1 << 2,
    Engine = 1 << 3,
};
struct Event
{
    bool Handled                                         = false;
    virtual ~Event()                                     = default;
    [[nodiscard]] virtual EventCategory category() const = 0;
};

// glfw events etc
struct WindowEvent : Event
{
    [[nodiscard]] EventCategory category() const override { return EventCategory::Window; }
};

struct MouseEvent : Event
{
    [[nodiscard]] EventCategory category() const override { return EventCategory::Mouse; }
};

struct KeyEvent : Event
{
    [[nodiscard]] EventCategory category() const override { return EventCategory::Key; }
};

struct EngineEvent : Event
{
    [[nodiscard]] EventCategory category() const override { return EventCategory::Engine; }
};


struct KeyPressedEvent : KeyEvent
{
    int  key{};
    bool repeat{};
};

struct MouseMovedEvent : MouseEvent
{
    double x{}, y{};
};
