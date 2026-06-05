# TODO: apple and windows support
if (UNIX AND NOT APPLE)
    add_executable(HotReloadApp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/hot_reloader.cpp
            src/ConfigLoader.cpp
    )

    target_link_libraries(HotReloadApp PRIVATE
            SD
            spdlog::spdlog
            ${CMAKE_DL_LIBS}
    )

    target_include_directories(HotReloadApp PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/vendor/tomlplusplus/include
    )
    # allows u to run without LD_LIBRARY_PATH if libSD.so is next to it :)
    set_target_properties(HotReloadApp PROPERTIES
            BUILD_RPATH "$ORIGIN:$ORIGIN/../lib"
            BUILD_RPATH_USE_ORIGIN TRUE
    )

    target_compile_definitions(HotReloadApp PRIVATE
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
            TOML_EXCEPTIONS=0
    )

    #~ INSTALL
    install(TARGETS HotReloadApp
            RUNTIME DESTINATION bin
    )

    add_custom_command(TARGET HotReloadApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/SD/config/engine.toml"
            "$<TARGET_FILE_DIR:HotReloadApp>/engine.toml"
            COMMENT "Deploy engine config"
            VERBATIM
    )


    if (SD_ENABLE_ASAN)
        if (MSVC)
            message(WARNING "SD_ENABLE_ASAN is ON, but ASan is not configured for MSVC yet.")
        else ()
            target_compile_options(HotReloadApp PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(HotReloadApp PRIVATE -fsanitize=address)
        endif ()
    endif ()
endif ()