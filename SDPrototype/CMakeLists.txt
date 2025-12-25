add_executable(SDPrototype main.cpp)
find_package(Vulkan REQUIRED)
target_link_libraries(SDPrototype PRIVATE nvrhi_vk nvrhi Vulkan::Vulkan glfw spdlog stb_image dxc)


target_compile_definitions(SDPrototype PRIVATE
        # Debug-only definitions
        $<$<CONFIG:Debug>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE>
        $<$<CONFIG:Debug>:_DEBUG>

        # Release-only definitions
        $<$<CONFIG:Release>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO>
        $<$<CONFIG:Release>:NDEBUG>
)


add_custom_command(TARGET SDPrototype POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/vendor/dxc/lib/libdxcompiler.so"
        $<TARGET_FILE_DIR:SDPrototype>
        COMMENT "Copying DXC shared library to output directory"
)

if (MSVC)
    target_compile_options(SDPrototype PRIVATE
            $<$<CONFIG:Release>:/O2 /Ob2>
            $<$<CONFIG:Debug>:/Od /Zi>
    )
else ()
    target_compile_options(SDPrototype PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native>
            $<$<CONFIG:Debug>:-g -O0>
    )
endif ()