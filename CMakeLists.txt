cmake_minimum_required(VERSION 4.0)
project(SD
    VERSION 0.0.1
    DESCRIPTION "Shader Debug"
    LANGUAGES CXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Completely disable P1689 dependency scanning (no clang-scan-deps in rules)
  unset(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS CACHE)
  set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "" CACHE STRING "" FORCE)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Ensure no clang-scan-deps tool is wired into the build graph
  unset(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS CACHE)
  set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "" CACHE STRING "" FORCE)
endif ()


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_subdirectory(vendor)
add_subdirectory(SDEngine)
