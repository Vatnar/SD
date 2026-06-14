cmake_minimum_required(VERSION 3.25...4.0)

project(SD
        VERSION 0.0.1
        DESCRIPTION "Engine"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SD_ENABLE_ASAN "Enable AddressSanitizer on supported toolchains" OFF)
option(SD_ENABLE_IWYU "Enable IWYU helper targets/scripts when available" ON)
option(SD_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(SD_ENABLE_CCACHE "Enable ccache compiler launcher when available" ON)


#~ caching
if (SD_ENABLE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if (CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
    else ()
        message(STATUS "ccache not found, install with: sudo apt install ccache")
    endif ()
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
if (UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif ()

# TODO: probably shouldnt be global
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


add_subdirectory(vendor)
add_subdirectory(SD)
add_subdirectory(hot_reloader)
add_subdirectory(testing)

# TODO: we probably  shouldnt even have sandbox app here, sandboxapp should have SD as depdenceny not opposite
add_subdirectory(sandbox_app)

