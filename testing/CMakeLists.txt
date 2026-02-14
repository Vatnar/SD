add_executable(SDTesting
        TestECS.cpp
)
target_link_libraries(SDTesting PRIVATE
        SDEngineLib
)