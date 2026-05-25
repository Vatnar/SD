
add_subdirectory(glfw)

# Disable exceptions in spdlog so it aborts instead of throwing through -fno-exceptions code
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(spdlog)
add_subdirectory(stb)
add_subdirectory(dxc)
add_subdirectory(VLA)
add_subdirectory(tomlplusplus)

# Disable unity builds for vendor libraries that don't support them
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_UNITY_BUILD)
    # glfw and spdlog need unity disabled due to macro conflicts
    # Must be set BEFORE they are added as dependencies
    set_target_properties(glfw spdlog PROPERTIES UNITY_BUILD OFF)
endif()

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

target_include_directories(imgui
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)
target_link_libraries(imgui PRIVATE glfw)

# Unity builds for ImGui - Release only
# Core ImGui files benefit from unity builds, but backends have macro conflicts
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_UNITY_BUILD)
    message(STATUS "ImGui: Unity build enabled (batch size 4)")
    set_target_properties(imgui PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE 4
    )
    
    # Exclude backends with X11/Vulkan macro conflicts from unity builds
    set_source_files_properties(
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "ImGui: Unity build disabled")
    endif()
endif()
