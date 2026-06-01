cmake_minimum_required(VERSION 3.25...4.0)
project(SD
        VERSION 0.0.1
        DESCRIPTION "Shader Debug"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_USE_STD_EXPECTED=1
        VULKAN_HPP_NO_EXCEPTIONS=1
)

# Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found, install with: sudo apt install ccache")
endif()

# Unity builds - enable globally for Release, disable for Debug
# Per-target exclusions are set in vendor/CMakeLists.txt for incompatible libraries
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)
    message(STATUS "Unity builds enabled for Release")
else()
    set(CMAKE_UNITY_BUILD OFF)
    message(STATUS "Unity builds disabled for ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(vendor)
add_subdirectory(SD)

add_subdirectory(testing)

add_subdirectory(SandboxApp)


install(TARGETS HotReloadApp
        RUNTIME DESTINATION bin)

install(TARGETS SandboxApp
        LIBRARY DESTINATION bin)

install(DIRECTORY "${SD_SOURCE_DIR}/engine_assets" DESTINATION bin/assets/engine)
install(DIRECTORY "${SandboxApp_SOURCE_DIR}/assets" DESTINATION bin/assets/)
