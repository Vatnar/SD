add_subdirectory(glfw)

# Disable exceptions in spdlog so it aborts instead of throwing through -fno-exceptions code
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(spdlog)
add_subdirectory(stb)
add_subdirectory(dxc)
add_subdirectory(VLA)
add_subdirectory(tomlplusplus)

# glfw and spdlog have macro conflicts in unity builds; these exclusions
# are safe to set unconditionally — they are no-ops when unity is off.
set_target_properties(glfw spdlog PROPERTIES UNITY_BUILD OFF)

add_library(imgui STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

target_link_libraries(imgui PRIVATE glfw VLA)

# Unity build settings for imgui are controlled via CMAKE_UNITY_BUILD
# in the preset. Backends are excluded unconditionally due to X11/Vulkan
# macro conflicts — safe to set even when unity is off.
set_source_files_properties(
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

add_library(stb STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/stbi_impl.cpp
)
target_include_directories(stb
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/stb/include
)
set_target_properties(stb PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        POSITION_INDEPENDENT_CODE ON
)

add_library(vma STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/vma_impl.cpp
)
target_include_directories(vma
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/VMA/include
)
set_target_properties(vma PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        POSITION_INDEPENDENT_CODE ON
)
