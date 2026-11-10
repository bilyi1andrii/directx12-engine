#include <system_error>

#include <gtest/gtest.h>

#include "core/Utf8.h"

using engine::core::PathFromUtf8;

TEST(PathFromUtf8, KeepsNonAsciiNames) {
    // "Жовтий.png" in UTF-8; read in the ANSI code page it would come out garbled
    const auto path = PathFromUtf8("\xD0\x96\xD0\xBE\xD0\xB2\xD1\x82\xD0\xB8\xD0\xB9.png");

    EXPECT_EQ(path.native(), L"Жовтий.png");
}

TEST(PathFromUtf8, InvalidUtf8Throws) {
    // 0xFF never appears in UTF-8
    EXPECT_THROW((void)PathFromUtf8("bad\xFF.png"), std::system_error);
}
