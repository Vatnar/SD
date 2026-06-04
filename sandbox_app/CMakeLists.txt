###############
# Game module #
###############

add_library(SandboxGameObjects OBJECT
        GameRenderLayer.cpp
)

target_link_libraries(SandboxGameObjects PRIVATE
        SD
)

if (MSVC)
    target_compile_options(SandboxGameObjects PRIVATE
            $<$<CONFIG:Debug>:/Od /Zi /EHsc->
            $<$<CONFIG:Release>:/O2 /Ob2 /EHsc->
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /EHsc->
            /W4
            /permissive-
            $<$<BOOL:${SD_WARNINGS_AS_ERRORS}>:/WX>
    )
else ()
    target_compile_options(SandboxGameObjects PRIVATE
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

if (SD_ENABLE_ASAN AND NOT MSVC)
    target_compile_options(SandboxGameObjects PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
    )
endif ()

#####################################
# Hot-reload plugin (Linux only)    #
#####################################
if (UNIX AND NOT APPLE)
    add_library(SandboxApp SHARED
            sandbox.cpp
            $<TARGET_OBJECTS:SandboxGameObjects>
    )

    target_compile_definitions(SandboxApp PRIVATE
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
            VULKAN_HPP_NO_CONSTRUCTORS
            VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
            VULKAN_HPP_USE_STD_EXPECTED=1
            VULKAN_HPP_NO_EXCEPTIONS=1
    )

    target_link_libraries(SandboxApp PRIVATE
            SD
    )


    if (MSVC)
        target_compile_options(SandboxApp PRIVATE
                $<$<CONFIG:Debug>:/Od /Zi /EHsc->
                $<$<CONFIG:Release>:/O2 /Ob2 /EHsc->
                $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /EHsc->
                /W4
                /permissive-
                $<$<BOOL:${SD_WARNINGS_AS_ERRORS}>:/WX>
        )
    else ()
        target_compile_options(SandboxApp PRIVATE
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

    if (SD_ENABLE_ASAN AND NOT MSVC)
        target_compile_options(SandboxApp PRIVATE
                -fsanitize=address
                -fno-omit-frame-pointer
        )
        target_link_options(SandboxApp PRIVATE
                -fsanitize=address
        )
    endif ()

    install(TARGETS SandboxApp
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
    )
endif ()

######################
# Standalone         #
######################
add_executable(SandboxAppStandalone
        main.cpp
        $<TARGET_OBJECTS:SandboxGameObjects>
)

target_link_libraries(SandboxAppStandalone PRIVATE
        SD
)

set_target_properties(SandboxAppStandalone PROPERTIES
        BUILD_RPATH "$ORIGIN:$ORIGIN/../lib"
        BUILD_RPATH_USE_ORIGIN TRUE
)

set_target_properties(SandboxApp PROPERTIES
        BUILD_RPATH "$ORIGIN"
        BUILD_RPATH_USE_ORIGIN TRUE
)

if (MSVC)
    target_compile_options(SandboxAppStandalone PRIVATE
            $<$<CONFIG:Debug>:/Od /Zi /EHsc->
            $<$<CONFIG:Release>:/O2 /Ob2 /EHsc->
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /EHsc->
            /W4
            /permissive-
            $<$<BOOL:${SD_WARNINGS_AS_ERRORS}>:/WX>
    )
else ()
    target_compile_options(SandboxAppStandalone PRIVATE
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

if (SD_ENABLE_ASAN AND NOT MSVC)
    target_compile_options(SandboxAppStandalone PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
    )
    target_link_options(SandboxAppStandalone PRIVATE
            -fsanitize=address
    )
endif ()

install(TARGETS SandboxAppStandalone
        RUNTIME DESTINATION bin
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            DESTINATION bin/assets
    )
endif ()

add_custom_target(sandbox_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
        COMMENT "Sync Sandbox assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/hotreload.toml"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hotreload.toml"

)

add_dependencies(SandboxAppStandalone sandbox_assets)
if (TARGET SandboxApp)
    add_dependencies(SandboxApp sandbox_assets)
endif ()