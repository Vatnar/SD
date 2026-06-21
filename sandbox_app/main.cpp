#include <SD/Application.hpp>
#include <SD/core/logging.hpp>
#include <SD/game_api.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

int main() {
  sd::log::init();

  sd::ApplicationSpecification spec{
      .name            = "Sandbox",
      .width           = 1280,
      .height          = 720,
      .enableHotReload = false,
  };
  sd::Application app{spec};

  game::State state{};
  game::on_load(app, state);

  while (app.is_running) {
    app.frame();
  }

  game::on_unload(app, state);

  return 0;
}
