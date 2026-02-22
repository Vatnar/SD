#pragma once

#include <string>
#include <vector>

namespace SD {

class Application;
class Scene;

class GameContext {
public:
    virtual ~GameContext() = default;

    virtual void OnLoad(Application& app) = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnUnload() = 0;

    virtual std::vector<Scene*> GetScenes() = 0;
};

} // namespace SD
