include(FetchContent)
# TESTING
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(SD_TEST_SOURCES
        tests/main.cpp
        tests/ecs_tests.cpp
        tests/CommandQueueTest.cpp
        tests/FileSerializationTest.cpp
)
add_executable(SDGTest ${SD_TEST_SOURCES})
target_link_libraries(SDGTest PRIVATE
        SD
        GTest::gtest
)

# Tests need exceptions (GoogleTest requirement); override -fno-exceptions inherited from SD
target_compile_options(SDGTest PRIVATE -fexceptions)

if (SD_ENABLE_ASAN)
    if (NOT MSVC)
        target_compile_options(SDGTest PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(SDGTest PRIVATE -fsanitize=address)
    else ()
        message(WARNING "SD_ENABLE_ASAN is ON, but ASan is not configured for MSVC yet.")
    endif ()
endif ()

include(GoogleTest)
gtest_discover_tests(SDGTest)