#include "test_helpers.h"

// Runs every TEST(...) in the other files and prints a summary.
// Exit code is 0 if everything passed, 1 otherwise (that is what ctest looks at).
int main() {
    int failedTests = 0;
    for (const auto& test : allTests()) {
        failuresInCurrentTest() = 0;
        test.run();
        if (failuresInCurrentTest() == 0) {
            std::cout << "[ PASS ] " << test.name << "\n";
        } else {
            std::cout << "[ FAIL ] " << test.name << "\n";
            failedTests++;
        }
    }
    std::cout << "\n" << (allTests().size() - failedTests) << " of " << allTests().size() << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
}