#include <gtest/gtest.h>

#include "core/ComScope.h"

int main(int argc, char **argv) {
    // One for the whole process, not one per test: see core/ComScope.h
    const core::ComScope com;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
