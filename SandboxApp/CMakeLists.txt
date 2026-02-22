# SCRATCH
add_library(SandboxApp SHARED SDScratch.cpp
        GameRenderLayer.cpp)

target_link_libraries(SandboxApp PRIVATE SD)
add_custom_command(TARGET SandboxApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SandboxApp>
        "${CMAKE_BINARY_DIR}/bin/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/bin/assets/"
        COMMENT "Deploy SandboxApp.so + assets to bin/"
        VERBATIM
)
add_dependencies(SandboxApp SD)

# Standalone executable (no hot reload)
add_executable(SandboxAppStandalone main.cpp GameRenderLayer.cpp)
target_link_libraries(SandboxAppStandalone PRIVATE SD)
add_custom_command(TARGET SandboxAppStandalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/bin/assets/"
        COMMENT "Deploy assets to bin/"
        VERBATIM
)
