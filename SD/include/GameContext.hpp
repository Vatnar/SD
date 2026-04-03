// TODO(docs): Add file-level Doxygen header
//   - @file GameContext.hpp
//   - @brief Interface for game-specific code in hot reload scenarios
//   - How this enables game.dll separation from engine
#pragma once

#include <string>
#include <vector>

namespace SD {

class Application;
class Scene;

// TODO(docs): Document GameContext interface
//   - Purpose: Abstract interface for game code that can be hot-reloaded
//   - Lifecycle: OnLoad -> OnUpdate (loop) -> OnUnload
//   - How to implement a game using this interface
//   - Example minimal game implementation
class GameContext {
public:
    virtual ~GameContext() = default;

    virtual void OnLoad(Application& app) = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnUnload() = 0;

    virtual std::vector<Scene*> GetScenes() = 0;
};

} // namespace SD
