#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct SD_Application SD_Application;
typedef struct SD_Scene SD_Scene;
typedef struct SD_RuntimeStateManager SD_RuntimeStateManager;

// Game state - allocated by platform, filled by game
typedef struct GameState {
    SD_Scene* sharedScene;
    SD_Scene* anotherScene;
    int version;  // For testing reloads
} GameState;

// Function pointer types
typedef void (*Game_OnLoadFn)(SD_Application* app, GameState* state);
typedef void (*Game_OnUpdateFn)(SD_Application* app, GameState* state, float dt);
typedef void (*Game_OnUnloadFn)(SD_Application* app, GameState* state);

// The game API - function pointers we reload
typedef struct GameAPI {
    Game_OnLoadFn OnLoad;
    Game_OnUpdateFn OnUpdate;
    Game_OnUnloadFn OnUnload;
} GameAPI;

// The single exported function from the game DLL
typedef GameAPI (*GetGameAPIFn)(void);

// Name of the exported function
#define GAME_GET_API_NAME "GetGameAPI"

#ifdef __cplusplus
}
#endif
