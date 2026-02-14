#include "Core/Logging.hpp"
#include "gtest/gtest.h"
int main(int argc, char** argv) {
  init_logging();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
