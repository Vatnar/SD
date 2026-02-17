#include "Application.hpp"
#include "Core/Logging.hpp"

int main() {
  SD::Log::Init();

  SD::Application app;
  app.Run();
}
