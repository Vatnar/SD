# SCRATCH
include(FetchContent)
add_executable(SDScratch
        SDScratch.cpp
)
target_link_libraries(SDScratch PRIVATE
        SDEngineLib
)

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
        SDEngineLib
        GTest::gtest
)
include(GoogleTest)
gtest_discover_tests(SDGTest)
