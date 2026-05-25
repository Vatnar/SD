// IWYU Test File
// This file tests IWYU functionality

#include <string>
#include <vector>
#include <deque>

namespace SD {
namespace Test {

void TestFunction() {
    std::vector<int> vec;
    std::deque<int> deq;
    std::string str = "test";
    // Note: <iostream> is included but not used
    // Note: <deque> is used and must be included
}

} // namespace Test
} // namespace SD
