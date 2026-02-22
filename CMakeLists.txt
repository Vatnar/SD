cmake_minimum_required(VERSION 3.25...4.0)
project(SD
        VERSION 0.0.1
        DESCRIPTION "Shader Debug"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unity builds - disabled due to imgui Vulkan backend include order issues
# set(CMAKE_UNITY_BUILD ON)
# set(CMAKE_UNITY_BUILD_BATCH_SIZE 0)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(vendor)
add_subdirectory(SD)

add_subdirectory(testing)

add_subdirectory(SandboxApp)


install(TARGETS HotReloadApp SandboxApp
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION bin)

install(DIRECTORY "${SD_SOURCE_DIR}/engine_assets" DESTINATION bin/assets/engine)
install(DIRECTORY "${SandboxApp_SOURCE_DIR}/assets" DESTINATION bin/assets/)
