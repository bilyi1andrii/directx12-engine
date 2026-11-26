// Tests turning UTF-8 text into paths.

#include <gtest/gtest.h>

#include "core/Utf8.h"

using core::PathFromUtf8;

TEST(PathFromUtf8, KeepsNonAsciiNames) {
    const auto path = PathFromUtf8("\xD0\x96\xD0\xBE\xD0\xB2\xD1\x82\xD0\xB8\xD0\xB9.png");

    EXPECT_EQ(path.native(), L"Жовтий.png");
}
