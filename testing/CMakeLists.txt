include(FetchContent)
# TESTING
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
)
FetchContent_MakeAvailable(googletest)


enable_testing()

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(SDGTest ${TEST_SOURCES})
target_link_libraries(SDGTest PRIVATE
        SD
        GTest::gtest
)

# Tests need exceptions (GoogleTest requirement); override -fno-exceptions inherited from SD
target_compile_options(SDGTest PRIVATE -fexceptions)

# AddressSanitizer - Debug builds only
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(SDGTest PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(SDGTest PRIVATE -fsanitize=address)
endif()
include(GoogleTest)
gtest_discover_tests(SDGTest)
