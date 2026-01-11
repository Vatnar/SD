#pragma once
#include "spdlog/spdlog.h"


#include <list>
#include <Layer.hpp>


struct Vertex
{
    float position[3];
    float texCoord[2];
};


class LayerList

{
public:
    using Container = std::list<std::unique_ptr<Layer>>;

    ~LayerList()
    {
        for (auto& layer : mLayers)
        {
            layer->OnDetach();
        }
    }

    // TODO: possibly a fixed update could be nice in the future aswell
    void Update(const float dt) const
    {
        for (auto& layer : mLayers)
            if (layer->IsActive())
                layer->OnUpdate(dt);
    }
    void Render() const
    {
        for (const auto& layer : mLayers)
        {
            if (layer->IsActive())
                layer->OnRender();
        }
    }

    void HandleEvent(Event& e)
    {
        for (auto it = mLayers.rbegin(); it != mLayers.rend(); ++it)
        {
            if ((*it)->IsActive())
            {
                (*it)->OnEvent(e);
                if (e.Handled)
                    break;
            }
        }
    }

    template<IsLayer T, typename... Args>
    void CreateAndAttachTop(Args&&...args)
    {
        auto layer = std::make_unique<T>(std::forward<Args>(args)...);
        T&   ref   = *layer;
        mLayers.emplace_back(std::move(layer));
        ref.OnAttach();
    }

    template<IsLayer T, typename... Args>
    void CreateAndAttachBottom(Args&&...args)
    {
        auto layer = std::make_unique<T>(std::forward<Args>(args)...);
        T&   ref   = *layer;
        mLayers.emplace_front(std::move(layer));
        ref.OnAttach();
    }
    template<IsLayer T>
    void AttachTop(std::unique_ptr<T> layer)
    {
        T& ref = *layer;
        mLayers.emplace_back(std::move(layer));
        ref.OnAttach();
    }

    template<IsLayer T>
    void AttachBottom(std::unique_ptr<T> layer)
    {
        T& ref = *layer;
        mLayers.emplace_front(std::move(layer));
        ref.OnAttach();
    }

    template<IsLayer T>
    T *GetRef()
    {
        for (auto it = mLayers.begin(); it != mLayers.end(); ++it)
        {
            if (auto p = dynamic_cast<T *>(it->get()))
            {
                return p;
            }
        }
        return nullptr;
    }

    template<IsLayer T>
    std::unique_ptr<T> DetachFirst()
    {
        for (auto it = mLayers.begin(); it != mLayers.end(); ++it)
        {
            if (dynamic_cast<T *>(it->get()) != nullptr)
            {
                (*it)->OnDetach();

                std::unique_ptr<Layer> base = std::move(*it);
                mLayers.erase(it);

                return std::unique_ptr<T>(static_cast<T *>(base.release()));
            }
        }
        return nullptr;
    }

private:
    Container mLayers;
};
