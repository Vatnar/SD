##################
# stb image impl #
##################
add_library(stb STATIC src/vendor/stbi_impl.cpp)
target_include_directories(stb PUBLIC ${PROJECT_SOURCE_DIR}/vendor/stb/include)
set_target_properties(stb PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)

#############
# SD ENGINE #
#############
set(ENGINE_SOURCES
        src/Core/VulkanContext.cpp
        src/Core/Window.cpp
        src/Core/Layer.cpp
        src/Layers/ImGuiLayer.cpp
        src/Layers/TestLayer.cpp
        src/Layers/Shader2DLayer.cpp
        src/Renderer/Vertex.cpp
)

add_library(SDEngineLib STATIC ${ENGINE_SOURCES})
add_executable(SDEngine src/main.cpp)

target_link_libraries(SDEngine PRIVATE SDEngineLib)


find_package(Vulkan REQUIRED)
if (UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif ()


target_link_libraries(SDEngineLib PUBLIC
        imgui glfw spdlog stb dxc
        VLA
        Vulkan::Vulkan
        $<$<PLATFORM_ID:Linux>:X11::X11>
)

target_include_directories(SDEngineLib PUBLIC include)

################
# SDEngine PCH #
################
target_precompile_headers(SDEngineLib PRIVATE
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

target_compile_definitions(SDEngineLib PUBLIC
        # Debug-only definitions
        $<$<CONFIG:Debug>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE>
        $<$<CONFIG:Debug>:_DEBUG>

        # Release-only definitions
        $<$<CONFIG:Release>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO>
        $<$<CONFIG:Release>:NDEBUG>
)


if (MSVC)
    target_compile_options(SDEngineLib PUBLIC
            $<$<CONFIG:Release>:/O2 /Ob2>
            $<$<CONFIG:Debug>:/Od /Zi>
    )
else ()
    target_compile_options(SDEngineLib PUBLIC
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Debug>:-g -O0>
    )
endif ()