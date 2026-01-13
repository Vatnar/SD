#pragma once

enum class InputEventCategory
{
    MousePressed,
    MouseReleased,
    KeyPressed,
    KeyReleased,
    MouseScroll,
    CursorPos
};
struct InputEvent
{
    bool Handled{false};
    virtual ~InputEvent()                                     = default;
    [[nodiscard]] virtual InputEventCategory category() const = 0;
};

struct MousePressedEvent : InputEvent
{
    int  button{};
    int  mods{};
    bool repeat{};

    MousePressedEvent() = default;

    MousePressedEvent(int button, int mods, bool repeat) : InputEvent{}, button(button), mods(mods), repeat(repeat) {}
    [[nodiscard]] InputEventCategory category() const override { return InputEventCategory::MousePressed; }
};

struct MouseReleasedEvent : InputEvent
{
    int button{};
    int mods{};

    MouseReleasedEvent() = default;
    MouseReleasedEvent(int button, int mods) : InputEvent{}, button(button), mods(mods) {}
    [[nodiscard]] InputEventCategory category() const override { return InputEventCategory::MouseReleased; }
};

struct KeyPressedEvent : InputEvent
{
    int  key{};
    int  scancode{};
    int  mods{};
    bool repeat{};

    KeyPressedEvent() = default;
    KeyPressedEvent(int key, int scancode, int mods, bool repeat)
        : InputEvent{}, key(key), scancode(scancode), mods(mods), repeat(repeat)
    {}

    [[nodiscard]] InputEventCategory category() const override { return InputEventCategory::KeyPressed; }
};

struct KeyReleasedEvent : InputEvent
{
    int key{};
    int scancode{};
    int mods{};

    KeyReleasedEvent() = default;
    KeyReleasedEvent(int key, int scancode, int mods) : InputEvent{}, key(key), scancode(scancode), mods(mods) {}
    [[nodiscard]] InputEventCategory category() const override { return InputEventCategory::KeyReleased; }
};

struct ScrollEvent : InputEvent
{
    double xOffset{}, yOffset{};

    [[nodiscard]] InputEventCategory category() const override { return InputEventCategory::MouseScroll; }
    ScrollEvent() = default;
    ScrollEvent(double xOffset, double yOffset) : InputEvent{}, xOffset(xOffset), yOffset(yOffset) {}
};
struct CursorEvent : InputEvent
{
    double xPos{}, yPos{};

    [[nodiscard]] InputEventCategory category() const override { return InputEventCategory::CursorPos; }
    CursorEvent() = default;
    CursorEvent(double xPos, double yPos) : InputEvent{}, xPos(xPos), yPos(yPos) {}
};
