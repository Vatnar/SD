#ifdef Error
#undef Error
#endif

#include "Core/Logging.hpp"
#include "gtest/gtest.h"

int main(int argc, char** argv) {
  sd::log::init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}