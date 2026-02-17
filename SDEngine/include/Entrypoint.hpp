#pragma once
#include "Application.hpp"
#include "Core/Logging.hpp"

extern SD::Application* CreateApplication();


int main(int argc, char** argv) {
  SD::Log::Init();
  auto app = CreateApplication();
  app->Run();
  delete app;
}
