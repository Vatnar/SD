add_executable(SDEngine src/main.cpp src/VulkanContext.cpp src/Window.cpp
        src/Layer.cpp
        src/stbi_impl.cpp
)
find_package(Vulkan REQUIRED)
target_link_libraries(SDEngine PRIVATE nvrhi_vk nvrhi Vulkan::Vulkan glfw spdlog stb_image dxc)
target_include_directories(SDEngine PRIVATE include)


target_compile_definitions(SDEngine PRIVATE
        # Debug-only definitions
        $<$<CONFIG:Debug>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE>
        $<$<CONFIG:Debug>:_DEBUG>

        # Release-only definitions
        $<$<CONFIG:Release>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO>
        $<$<CONFIG:Release>:NDEBUG>
)


if (MSVC)
    target_compile_options(SDEngine PRIVATE
            $<$<CONFIG:Release>:/O2 /Ob2>
            $<$<CONFIG:Debug>:/Od /Zi>
    )
else ()
    target_compile_options(SDEngine PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native>
            $<$<CONFIG:Debug>:-g -O0>
    )
endif ()