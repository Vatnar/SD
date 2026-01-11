
add_subdirectory(glfw)
add_subdirectory(spdlog)
add_subdirectory(stb)
add_subdirectory(dxc)

add_library(imgui STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
)


target_include_directories(imgui
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

target_sources(imgui
        PRIVATE
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)