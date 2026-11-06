#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "TestFiles.h"
#include "assets/ModelImporter.h"
#include "core/Utf8.h"

using engine::assets::ImportModel;
using engine::core::ErrorCode;
using engine::tests::MissingFile;
using engine::tests::TempFile;

namespace {
    // The bytes of a text file, for TempFile
    [[nodiscard]] std::span<const std::byte> Bytes(std::string_view text) { return std::as_bytes(std::span(text)); }

    // One triangle in the XY plane
    constexpr std::string_view kTriangleObj = "v 0 0 0\n"
                                              "v 1 0 0\n"
                                              "v 0 1 0\n"
                                              "f 1 2 3\n";
} // namespace

TEST(ImportModel, MissingFileReturnsFileNotFound) {
    const auto path = MissingFile(L"engine_test_missing.obj");

    const auto result = ImportModel(path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::FileNotFound) << result.error().message;
    EXPECT_NE(result.error().message.find(engine::core::ToUtf8(path.filename().native())), std::string::npos)
        << result.error().message;
}

TEST(ImportModel, UnknownExtensionReturnsUnsupportedFormat) {
    const TempFile file(L"engine_test_model.xyz", Bytes(kTriangleObj));

    const auto result = ImportModel(file.Path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::UnsupportedFormat) << result.error().message;
}

TEST(ImportModel, DamagedFileReturnsDecodeFailed) {
    const TempFile file(L"engine_test_damaged.gltf", Bytes("{ \"asset\": "));

    const auto result = ImportModel(file.Path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DecodeFailed) << result.error().message;
}

TEST(ImportModel, ReadsObj) {
    const TempFile file(L"engine_test_triangle.obj", Bytes(kTriangleObj));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
}
