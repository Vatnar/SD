##################
# stb image impl #
##################
add_library(stb STATIC src/vendor/stbi_impl.cpp)
target_include_directories(stb PUBLIC ${PROJECT_SOURCE_DIR}/vendor/stb/include)
set_target_properties(stb PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        POSITION_INDEPENDENT_CODE ON
)
############
# VMA impl #
############
add_library(vma STATIC src/vendor/vma_impl.cpp)
target_include_directories(vma PUBLIC ${PROJECT_SOURCE_DIR}/vendor/VMA/include)
set_target_properties(vma PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        POSITION_INDEPENDENT_CODE ON
)
#############
# sd ENGINE #
#############
file(GLOB_RECURSE ECS_IMPL_headers "include/core/ecs/impl/*.inl")
set(ENGINE_SOURCES
        src/core/vulkan/VulkanContext.cpp
        src/core/Window.cpp
        src/core/layers/SDImGuiContext.cpp
        src/core/layers/EngineDebugLayer.cpp
        src/core/logging.cpp
        src/core/vulkan/VulkanRenderer.cpp
        src/core/vulkan/PipelineFactory.cpp
        src/core/vulkan/VulkanWindow.cpp
        src/core/vulkan/VulkanFramebuffer.cpp
        src/core/vulkan/ShaderLibrary.cpp
        src/core/SDImGuiViewport.cpp
        src/core/SceneManager.cpp
        src/core/LayoutManager.cpp
        src/Application.cpp
        src/core/WindowManager.cpp
        src/core/ViewManager.cpp
        src/core/View.cpp
        src/RuntimeStateManager.cpp
        src/core/ecs/CommandQueue.cpp
        src/core/ecs/ComponentFactory.cpp
        include/core/ecs/CommandQueue.hpp
        include/core/ecs/ComponentFactory.hpp
)

add_library(SD SHARED ${ENGINE_SOURCES} ${ECS_IMPL_headers})

find_package(Vulkan REQUIRED)
if (UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif ()


target_link_libraries(SD PUBLIC
        glfw imgui spdlog::spdlog
        stb vma
        dxc
        VLA
        Vulkan::Vulkan
        $<$<PLATFORM_ID:Linux>:X11::X11>
)

target_include_directories(SD PUBLIC
        include
        ${PROJECT_SOURCE_DIR}/vendor/VMA/include
)

################
# sdEngine PCH #
################
target_precompile_headers(SD PRIVATE
        <vulkan/vulkan.h>
        <GLFW/glfw3.h>
        <spdlog/spdlog.h>

        <vector>
        <string>
        <unordered_map>
        <array>
        <span>

        <memory>
        <functional>
        <optional>

        <algorithm>
        <cstdint>
)

target_compile_definitions(SD PRIVATE
        SPDLOG_COMPILED_LIB
        SPDLOG_NO_EXCEPTIONS
        # Debug-only definitions
        $<$<CONFIG:Debug>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE>

        # Release-only definitions
        $<$<CONFIG:Release>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO>
)
# PIC
set_target_properties(SD PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Unity builds - Release only for faster clean builds
# Debug uses ccache for fast incremental builds
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(SD PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE 8
    )
else ()
    set_target_properties(SD PROPERTIES UNITY_BUILD OFF)
endif ()

# AddressSanitizer - Debug builds only
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (MSVC)
        target_compile_options(SD PUBLIC -fsanitize=address)
    else ()
        target_compile_options(SD PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    endif ()
    target_link_options(SD PRIVATE -fsanitize=address)
endif ()

if (MSVC)
    target_compile_options(SD PUBLIC
            $<$<CONFIG:Release>:/O2 /Ob2 /EHsc->
            $<$<CONFIG:Debug>:/Od /Zi /EHsc->
    )
else ()
    target_compile_options(SD PRIVATE
            $<$<CONFIG:Release>:-O3 -fno-exceptions>
            $<$<CONFIG:Debug>:-g -O0 -fno-exceptions>
            -Wall -Wextra -Werror
    )
endif ()

# Hot reloading
add_executable(HotReloadApp src/hot_reloader.cpp src/ConfigLoader.cpp)
target_link_libraries(HotReloadApp PRIVATE SD dl spdlog::spdlog)
target_include_directories(HotReloadApp PRIVATE
        include
        ${PROJECT_SOURCE_DIR}/SandboxApp
        ${PROJECT_SOURCE_DIR}/vendor/tomlplusplus/include
)
target_compile_definitions(HotReloadApp PRIVATE SD_ENGINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Copy engine config to bin
add_custom_command(TARGET HotReloadApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "${CMAKE_BINARY_DIR}/bin/config"
        COMMENT "Deploy engine config to bin/"
        VERBATIM
)

# assets
add_custom_command(TARGET SD POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/bin/assets/engine"
        COMMENT "Deploy engine assets to bin/"
        VERBATIM
)
add_dependencies(HotReloadApp SandboxApp)

# AddressSanitizer - Debug builds only
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(HotReloadApp PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(HotReloadApp PRIVATE -fsanitize=address)
endif ()

target_compile_options(HotReloadApp PRIVATE -Wall -Wextra -Werror -fno-exceptions)
target_compile_definitions(HotReloadApp PRIVATE TOML_EXCEPTIONS=0)

# IWYU (Include What You Use) - Native CMake support
# To enable IWYU, configure with:
#   cmake -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE="iwyu_tool;-p;build-iwyu" ...
# Or use the iwyu-apply script from the project root

find_program(IWYU_TOOL iwyu_tool)
find_program(IWYU_TOOL_PY iwyu_tool.py)

if (IWYU_TOOL OR IWYU_TOOL_PY)
    set(IWYU_CMD ${IWYU_TOOL})
    if (NOT IWYU_CMD)
        set(IWYU_CMD ${IWYU_TOOL_PY})
    endif ()

    # Create a convenience script to run IWYU with proper settings
    file(WRITE ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "#!/bin/bash\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "# IWYU convenience script\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "# Usage: ./iwyu-apply.sh [build-dir]\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "BUILD_DIR=\${1:-build-iwyu}\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "echo 'Configuring with IWYU enabled...'\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "cmake -B \$BUILD_DIR -DCMAKE_EXPORT_COMPILE_COMMANDS=ON\\\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "    -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=\"${IWYU_CMD};-p;\$BUILD_DIR;-Xiwyu;--mapping_file=\\\${CMAKE_SOURCE_DIR}/iwyu.imp\"\\\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "    -DCMAKE_BUILD_TYPE=Debug\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "echo ''\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "echo 'Building with IWYU analysis...'\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "echo '(This will compile and run IWYU on each source file)'\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "cmake --build \$BUILD_DIR --target SD 2>&1 | tee iwyu-output.log\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "echo ''\n")
    file(APPEND ${CMAKE_SOURCE_DIR}/iwyu-apply.sh "echo 'IWYU output saved to: iwyu-output.log'\n")

    execute_process(COMMAND chmod +x ${CMAKE_SOURCE_DIR}/iwyu-apply.sh)

    # Test target for the IWYU test file
    add_custom_target(iwyu-test
            COMMAND ${CMAKE_COMMAND} -E echo "Testing IWYU on test file..."
            COMMAND ${CMAKE_COMMAND} -E echo "File: SD/src/Core/IWYU_Test.cpp"
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${IWYU_CMD} -p ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/core/IWYU_Test.cpp 2>&1
            | grep -v "bits/unicode-data.h"
            | grep -v "bits/mofunc_impl.h"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Testing IWYU on IWYU_Test.cpp..."
            VERBATIM
    )

    message(STATUS "IWYU available. Usage:")
    message(STATUS "  1. Run ./iwyu-apply.sh from project root")
    message(STATUS "  2. Or: cmake --build . --target iwyu-test")
    message(STATUS "  3. Or manually: cmake -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=\"iwyu_tool;-p;<build-dir>\" ...")
else ()
    message(STATUS "iwyu_tool not found. IWYU will not be available.")
    message(STATUS "Install with: sudo apt install iwyu")
endif ()