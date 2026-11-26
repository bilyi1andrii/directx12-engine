// Runs every unit test with COM started once for the whole process.

#include <gtest/gtest.h>

#include "core/ComScope.h"

int main(int argc, char **argv) {
    const core::ComScope com;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
