if (UNIX AND NOT APPLE)
    add_executable(arena_test
            test.cpp
    )

    target_link_libraries(arena_test PRIVATE
            SD
            quill::quill
    )


    target_compile_definitions(arena_test PRIVATE
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
            TOML_EXCEPTIONS=0
    )
    set_target_properties(arena_test PROPERTIES
            BUILD_RPATH "$ORIGIN:$ORIGIN/../lib"
            BUILD_RPATH_USE_ORIGIN TRUE
    )

    target_compile_options(arena_test PRIVATE
            -freflection
            -mclflushopt
    )

    if (SD_ENABLE_ASAN)
        if (NOT MSVC)
            target_compile_options(arena_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(arena_test PRIVATE -fsanitize=address)
        else ()
            message(WARNING "SD_ENABLE_ASAN is ON, but ASan is not configured for MSVC yet.")
        endif ()
    endif ()
endif ()