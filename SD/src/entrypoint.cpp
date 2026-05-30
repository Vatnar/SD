#include "Application.hpp"

extern sd::Application* CreateApplication(int, char**);
#include "Core/Logging.hpp"

int main(int argc, char** argv) {
  sd::log::init();
  auto app = CreateApplication(argc, argv);
  if (!app) {
    sd::log::engine::error("CreateApplication returned null");
    return 1;
  }
  app->run(nullptr);
  delete app;
}
