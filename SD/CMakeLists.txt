include(GenerateExportHeader)

set(SD_ENGINE_SOURCES
        src/Application.cpp
        src/RuntimeStateManager.cpp
        src/core/View.cpp
        src/core/Window.cpp
        src/core/logging.cpp
        src/core/SDImGuiViewport.cpp
        src/core/SceneManager.cpp
        src/core/LayoutManager.cpp
        src/core/WindowManager.cpp
        src/core/ViewManager.cpp
        src/core/layers/SDImGuiContext.cpp
        src/core/layers/EngineDebugLayer.cpp
        src/core/vulkan/VulkanContext.cpp
        src/core/vulkan/VulkanRenderer.cpp
        src/core/vulkan/VulkanWindow.cpp
        src/core/vulkan/VulkanFramebuffer.cpp
        src/core/vulkan/ShaderLibrary.cpp
        src/core/ecs/CommandQueue.cpp
        src/core/ecs/ComponentFactory.cpp
)
add_library(SD SHARED ${SD_ENGINE_SOURCES})

#~ ABI visibility
set_target_properties(SD PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

generate_export_header(SD
        BASE_NAME SD
        EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated/SD/export.hpp"
        STATIC_DEFINE SD_STATIC_DEFINE
)

#~ Target setup
target_include_directories(SD
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SD
        PUBLIC
        glfw
        stb
        VLA
        spdlog::spdlog
        imgui
        vma
        dxc
        Vulkan::Vulkan
        $<$<PLATFORM_ID:Linux>:X11::X11>

)

target_compile_definitions(SD
        PUBLIC
        SD_SUPPRESS_VULKAN_INFO
        SD_FAIL_ON_VULKAN_VALIDATION_ERROR

        SPDLOG_COMPILED_LIB
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
        VULKAN_HPP_NO_CONSTRUCTORS
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        VULKAN_HPP_USE_STD_EXPECTED=1
        VULKAN_HPP_NO_EXCEPTIONS=1
        SPDLOG_NO_EXCEPTIONS
)

if(SD_DEBUG_ENABLED)
  target_compile_definitions(SD PUBLIC SD_DEBUG SD_BUILD_DEBUG=1 ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE)
else()
  target_compile_definitions(SD PUBLIC ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO)
endif()

#~ pch
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

if (MSVC)
    target_compile_options(SD PRIVATE
            $<$<CONFIG:Debug>:/Od /Zi /EHsc->
            $<$<CONFIG:Release>:/O2 /Ob2 /EHsc->
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /EHsc->
            /W4
            /permissive-
            $<$<BOOL:${SD_WARNINGS_AS_ERRORS}>:/WX>
    )
else ()
    target_compile_options(SD PRIVATE
            $<$<CONFIG:Debug>:-g -O0>
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g>
            -fno-exceptions
            -Wall
            -Wextra
            -Wpedantic
            $<$<BOOL:${SD_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif ()


#~ INSTALL
install(TARGETS SD
        EXPORT SDTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION include)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/SD/export.hpp" DESTINATION include/SD)

#~ install assets
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            DESTINATION bin/assets/engine
    )
    message(STATUS "Registering install rule for SD assets to bin/assets/engine")
else ()
    message(WARNING "No assets directory for engine. Expected to find at ${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif ()

#~ ASAN
if (SD_ENABLE_ASAN)
    if (MSVC)
        message(WARNING "SD_ENABLE_ASAN is ON, but ASan is not configured for MSVC yet.")
    else ()
        target_compile_options(SD PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(SD PRIVATE -fsanitize=address)
    endif ()
endif ()


add_custom_target(sd_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/engine"
        COMMENT "Sync SD assets"
)

add_dependencies(SD sd_assets)


#~ IWYU TODO: this is kinda ugly
if (SD_ENABLE_IWYU)
    find_program(IWYU_TOOL NAMES iwyu_tool iwyu_tool.py)

    if (IWYU_TOOL)
        file(WRITE "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" "#!/bin/bash\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" "set -eu\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" "BUILD_DIR=\${1:-build-iwyu}\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" "cmake -S . -B \"\$BUILD_DIR\" \\\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \\\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" " -DCMAKE_BUILD_TYPE=Debug \\\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" " -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=\"${IWYU_TOOL};-p;\$BUILD_DIR;-Xiwyu;--mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp\"\n")
        file(APPEND "${CMAKE_SOURCE_DIR}/iwyu-apply.sh" "cmake --build \"\$BUILD_DIR\" --target SD 2>&1 | tee iwyu-output.log\n")
        execute_process(COMMAND chmod +x "${CMAKE_SOURCE_DIR}/iwyu-apply.sh")
    else ()
        message(STATUS "IWYU not found. Install with: sudo apt install iwyu")
    endif ()
endif ()
