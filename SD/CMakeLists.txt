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
# SD ENGINE #
#############
file(GLOB_RECURSE ECS_IMPL_headers "include/Core/ECS/impl/*.inl")
set(ENGINE_SOURCES
        src/Core/Vulkan/VulkanContext.cpp
        src/Core/Window.cpp
        src/Core/Layers/SDImGuiContext.cpp
        src/Core/Layers/EngineDebugLayer.cpp
        src/Core/Logging.cpp
        src/Core/Vulkan/VulkanRenderer.cpp
        src/Core/Vulkan/PipelineFactory.cpp
        src/Core/Vulkan/VulkanWindow.cpp
        src/Core/Vulkan/VulkanFramebuffer.cpp
        src/Core/SDImGuiViewport.cpp
        src/Application.cpp
        src/Core/WindowManager.cpp
        src/Core/ViewManager.cpp
        src/Core/Vulkan/ShaderLibrary.cpp
        src/Core/View.cpp
        src/RuntimeStateManager.cpp
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
# SDEngine PCH #
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
#########################
# SDEngine compile defs #
#########################

target_compile_definitions(SD PUBLIC
        SPDLOG_COMPILED_LIB
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_USE_STD_EXPECTED=1
        VULKAN_HPP_NO_EXCEPTIONS=1
        # Debug-only definitions
        $<$<CONFIG:Debug>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE>
        $<$<CONFIG:Debug>:_DEBUG>

        # Release-only definitions
        $<$<CONFIG:Release>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO>
        $<$<CONFIG:Release>:NDEBUG>
)
# PIC
set_target_properties(SD PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (MSVC)
    target_compile_options(SD PUBLIC
            -fsanitize=address
            $<$<CONFIG:Release>:/O2 /Ob2>
            $<$<CONFIG:Debug>:/Od /Zi>
    )
else ()
    target_compile_options(SD PUBLIC
            -fsanitize=address
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Debug>:-g -O0>
            -Wall -Wextra -Werror
    )
endif ()

target_link_options(SD PRIVATE
        -fsanitize=address
)

# Hot reloading
add_executable(HotReloadApp src/HotReloader.cpp src/ConfigLoader.cpp)
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

target_compile_options(HotReloadApp PRIVATE
        -fsanitize=address -Wall -Wextra -Werror)
target_link_options(HotReloadApp PRIVATE
        -fsanitize=address)