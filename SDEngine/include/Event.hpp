#pragma once
#include "Utils.hpp"


enum class EventCategory
{
    Window,
    Engine,
    MousePressed,
    MouseReleased,
    KeyPressed,
    KeyReleased,
    MouseScroll,
    CursorPos
};
struct Event
{
    bool Handled                                         = false;
    virtual ~Event()                                     = default;
    [[nodiscard]] virtual EventCategory category() const = 0;
};

struct WindowEvent : Event
{
    [[nodiscard]] EventCategory category() const override { return EventCategory::Window; }
};

struct EngineEvent : Event
{
    EngineEvent() : Event{} {}
    [[nodiscard]] EventCategory category() const override { return EventCategory::Engine; }
};

struct MousePressedEvent : Event
{
    int  button{};
    int  mods{};
    bool repeat{};

    MousePressedEvent() = default;

    MousePressedEvent(int button, int mods, bool repeat) : Event{}, button(button), mods(mods), repeat(repeat) {}
    [[nodiscard]] EventCategory category() const override { return EventCategory::MousePressed; }
};

struct MouseReleasedEvent : Event
{
    int button{};
    int mods{};

    MouseReleasedEvent() = default;
    MouseReleasedEvent(int button, int mods) : Event{}, button(button), mods(mods) {}
    [[nodiscard]] EventCategory category() const override { return EventCategory::MouseReleased; }
};

struct KeyPressedEvent : Event
{
    int  key{};
    int  scancode{};
    int  mods{};
    bool repeat{};

    KeyPressedEvent() = default;
    KeyPressedEvent(int key, int scancode, int mods, bool repeat)
        : Event{}, key(key), scancode(scancode), mods(mods), repeat(repeat)
    {}

    [[nodiscard]] EventCategory category() const override { return EventCategory::KeyPressed; }
};

struct KeyReleasedEvent : Event
{
    int key{};
    int scancode{};
    int mods{};

    KeyReleasedEvent() = default;
    KeyReleasedEvent(int key, int scancode, int mods) : Event{}, key(key), scancode(scancode), mods(mods) {}
    [[nodiscard]] EventCategory category() const override { return EventCategory::KeyReleased; }
};

struct ScrollEvent : Event
{
    double xOffset{}, yOffset{};

    [[nodiscard]] EventCategory category() const override { return EventCategory::MouseScroll; }
    ScrollEvent() = default;
    ScrollEvent(double xOffset, double yOffset) : Event{}, xOffset(xOffset), yOffset(yOffset) {}
};
struct CursorEvent : Event
{
    double xPos{}, yPos{};

    [[nodiscard]] EventCategory category() const override { return EventCategory::CursorPos; }
    CursorEvent() = default;
    CursorEvent(double xPos, double yPos) : Event{}, xPos(xPos), yPos(yPos) {}
};
