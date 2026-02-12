#include <stdio.h>

#define CTEST_IMPLEMENTATION
#include "ctest.h"

CTEST_CASE(foo) {
}

CTEST_CASE(bar) {
    CTEST_ASSERT_TRUE(false);
}

CTEST_CASE(baz) {
}

int main() {
    ctest_run_all();
    return 0;
}
