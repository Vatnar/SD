#include "Application.hpp"
#include "Core/Logging.hpp"

int main() {
  SD::init_logging();

  SD::Application app;
  app.Run();
}
