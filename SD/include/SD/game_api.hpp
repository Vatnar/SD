#pragma once

#include <stddef.h>
#include <stdint.h>

//~ C API (for dlopen / shared library boundary)

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_API_VERSION 1

typedef struct SD_Application SD_Application;
typedef struct SD_Scene       SD_Scene;

typedef struct GameState {
  SD_Scene* shared_scene;
  SD_Scene* another_scene;
  int       version;
} GameState;

typedef void (*Game_OnLoadFn)(SD_Application* app, GameState* state);
typedef void (*Game_OnUpdateFn)(SD_Application* app, GameState* state, float dt);
typedef void (*Game_OnUnloadFn)(SD_Application* app, GameState* state);

typedef struct GameAPI {
  uint32_t        api_version;
  size_t          struct_size;
  Game_OnLoadFn   on_load;
  Game_OnUpdateFn on_update;
  Game_OnUnloadFn on_unload;
} GameAPI;

typedef GameAPI (*GetGameAPIFn)();

#define GAME_GET_API_NAME "get_game_api"

#ifdef __cplusplus
} // extern "C"
#endif

//~ C++ API (for direct linking)

#ifdef __cplusplus

#include <SD/Application.hpp>

namespace game {

struct State {
  sd::Scene* shared_scene  = nullptr;
  sd::Scene* another_scene = nullptr;
  int        version       = 0;
};

void on_load(sd::Application& app, State& state);
void on_update(sd::Application& app, State& state, float dt);
void on_unload(sd::Application& app, State& state);

} // namespace game

#endif // __cplusplus
