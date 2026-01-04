#pragma once
#include <cstdint>
#include <vector>

enum class EventCategory : uint32_t
{
    Window      = 1 << 0,
    Mouse       = 1 << 1,
    Key         = 1 << 2,
    KeyPressed  = 1 << 3,
    KeyReleased = 1 << 4,
    Engine      = 1 << 5,
    MouseScroll = 1 << 6,
    CursorPos,
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
    int  scancode{};
    bool repeat{};
    // TODO: how do we show modifiers, maybe bool map or some shit
    // std::vector<int> mods;
    KeyPressedEvent() = default;
    KeyPressedEvent(int key, int scancode, bool repeat);

    [[nodiscard]] EventCategory category() const override { return EventCategory::KeyPressed; }
};

struct KeyReleasedEvent : KeyEvent
{
    int key{};
    int scancode{};
    KeyReleasedEvent() = default;
    KeyReleasedEvent(int key, int scancode);
    [[nodiscard]] EventCategory category() const override { return EventCategory::KeyReleased; }
};
inline KeyPressedEvent::KeyPressedEvent(int key, int scancode, bool repeat)
    : key(key), scancode(scancode), repeat(repeat)
{}
inline KeyReleasedEvent::KeyReleasedEvent(int key, int scancode) : key(key), scancode(scancode)
{}

struct ScrollEvent : MouseEvent
{
    double xOffset{}, yOffset{};

    [[nodiscard]] EventCategory category() const override { return EventCategory::MouseScroll; }
    ScrollEvent() = default;
    ScrollEvent(double xOffset, double yOffset) : MouseEvent{}, xOffset(xOffset), yOffset(yOffset) {}
};
struct CursorEvent : MouseEvent
{
    double xPos{}, yPos{};

    [[nodiscard]] EventCategory category() const override { return EventCategory::CursorPos; }
    CursorEvent() = default;
    CursorEvent(double xPos, double yPos) : MouseEvent{}, xPos(xPos), yPos(yPos) {}
};
