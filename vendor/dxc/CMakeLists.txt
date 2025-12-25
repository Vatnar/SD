add_library(dxc SHARED IMPORTED GLOBAL)

set_target_properties(dxc PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libdxcompiler.so"
)

target_include_directories(dxc INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
