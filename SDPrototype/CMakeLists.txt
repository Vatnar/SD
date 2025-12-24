add_executable(SDPrototype main.cpp)

find_package(Vulkan REQUIRED)
target_include_directories(SDPrototype PRIVATE
        ../vendor/NVRHI/thirdparty/Vulkan-Headers/include
        ../vendor/NVRHI/include
        ../vendor/glfw/include
        ../vendor/spdlog/include
)
target_link_libraries(SDPrototype PRIVATE nvrhi_vk nvrhi Vulkan::Vulkan glfw spdlog)

target_compile_definitions(SDPrototype PRIVATE
        # Debug-only definitions
        $<$<CONFIG:Debug>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_TRACE>
        $<$<CONFIG:Debug>:_DEBUG>

        # Release-only definitions
        $<$<CONFIG:Release>:ENGINE_LOG_LEVEL=ENGINE_LOG_LEVEL_INFO>
        $<$<CONFIG:Release>:NDEBUG> # Standard convention to disable asserts
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