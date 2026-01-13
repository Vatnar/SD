#pragma once
#include "InputEvent.hpp"

#include <concepts>


class Layer;
template<typename T> concept IsLayer = std::derived_from<T, Layer>;

class Layer
{
public:
    virtual ~Layer() = default;
    Layer()          = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}

    virtual void OnActivate() {}
    virtual void OnDeactivate() {}

    virtual void OnEvent(InputEvent&) {}
    virtual void OnSwapchainRecreated() {}

    virtual void OnRender();
    virtual void OnUpdate(float dt);

    [[nodiscard]] bool IsActive() const noexcept { return mActive; }

protected:
    bool mActive{true};
};
