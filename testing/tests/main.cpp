#ifdef Error
#undef Error
#endif

#include <vulkan/vulkan.hpp>

#include "SD/core/logging.hpp"
#include "gtest/gtest.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

int main(int argc, char** argv) {
  sd::log::init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
