#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include "TestFiles.h"
#include "core/File.h"

using core::ErrorCode;
using core::ReadBinaryFile;
using tests::MissingFile;
using tests::TempFile;

TEST(ReadBinaryFile, ReturnsTheFileContent) {
    constexpr std::array content{std::byte{1}, std::byte{0}, std::byte{255}};
    const TempFile file(L"engine_test_read.bin", content);

    const auto result = ReadBinaryFile(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->size(), content.size());
    EXPECT_EQ((*result)[2], std::byte{255});
}

TEST(ReadBinaryFile, MissingFileReturnsFileNotFound) {
    const auto result = ReadBinaryFile(MissingFile(L"engine_test_missing.bin"));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::FileNotFound);
    EXPECT_NE(result.error().message.find("engine_test_missing.bin"), std::string::npos);
}
