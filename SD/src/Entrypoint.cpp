
#include "Application.hpp"

extern SD::Application* CreateApplication(int, char**);
#include "Core/Logging.hpp"

int main(int argc, char** argv) {
  SD::Log::Init();
  auto app = CreateApplication(argc, argv);
  app->Run(nullptr);
  delete app;
}
