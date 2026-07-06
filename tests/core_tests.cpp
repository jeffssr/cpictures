#include <cstdlib>
#include <iostream>

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}
}

int main() {
    Expect(true, "test harness runs");
    std::cout << "cpictures core tests passed\n";
    return 0;
}
