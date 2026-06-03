// TODO(docs): Add file-level Doxygen header
//   - @file GameContext.hpp
//   - @brief Interface for game-specific code in hot reload scenarios
//   - How this enables game.dll separation from engine
#pragma once

#include <string>
#include <vector>

namespace sd {

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

    virtual void on_load(Application& app) = 0;
    virtual void on_update(float dt) = 0;
    virtual void on_unload() = 0;

    virtual std::vector<Scene*> get_scenes() = 0;
};

} // namespace SD
