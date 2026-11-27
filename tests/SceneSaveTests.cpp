// Tests reading and writing the sandbox's save file, and refusing broken ones.

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "TestFiles.h"
#include "sandbox/SceneSave.h"

namespace fs = std::filesystem;

using core::ErrorCode;
using sandbox::ReadSandboxSave;
using sandbox::SandboxSave;
using sandbox::WriteSandboxSave;
using tests::MissingFile;
using tests::TempFile;

namespace {
    [[nodiscard]] std::span<const std::byte> Bytes(std::string_view text) {
        return std::as_bytes(std::span(text));
    }

    [[nodiscard]] DirectX::XMFLOAT4X4 Matrix(float first) {
        DirectX::XMFLOAT4X4 matrix{};
        for (int i = 0; i < 16; ++i) {
            matrix.m[i / 4][i % 4] = first + static_cast<float>(i) * 0.25f;
        }
        return matrix;
    }

    void ExpectEqual(const DirectX::XMFLOAT4X4 &actual, const DirectX::XMFLOAT4X4 &expected) {
        for (int i = 0; i < 16; ++i) {
            EXPECT_FLOAT_EQ(actual.m[i / 4][i % 4], expected.m[i / 4][i % 4]) << "element " << i;
        }
    }

    class TempFolder {
    public:
        explicit TempFolder(const fs::path &name) : m_path(fs::temp_directory_path() / name) {
            std::error_code ignored;
            fs::remove_all(m_path, ignored);
        }
        ~TempFolder() {
            std::error_code ignored;
            fs::remove_all(m_path, ignored);
        }
        TempFolder(const TempFolder &) = delete;
        TempFolder &operator=(const TempFolder &) = delete;
        [[nodiscard]] const fs::path &Path() const noexcept { return m_path; }

    private:
        fs::path m_path;
    };

    [[nodiscard]] std::string ReadText(const fs::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] std::string SaveJson(std::string_view version, std::string_view rest = "") {
        return std::string(R"({ "version": )") + std::string(version) + std::string(rest) + " }";
    }
} // namespace

TEST(SandboxSave, WrittenSaveReadsBackTheSame) {
    const TempFolder root(L"engine_test_save_root");
    const fs::path outside = fs::temp_directory_path() / L"elsewhere" / L"model.obj";
    SandboxSave save{.gridCount = 400, .gridMode = "dynamic"};
    save.gridCubes.push_back({.index = 12, .world = Matrix(1.0f)});
    save.cubes.push_back({.name = "Orange cube", .world = Matrix(2.0f)});
    save.models.push_back({.path = root.Path() / L"assets" / L"замок" / L"scene.gltf", .world = Matrix(3.0f)});
    save.models.push_back({.path = outside, .world = Matrix(4.0f)});
    const fs::path file = root.Path() / L"saves" / L"sandbox.json";

    ASSERT_TRUE(WriteSandboxSave(file, save, root.Path()).has_value());
    const auto read = ReadSandboxSave(file, root.Path());

    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(read->gridCount, 400u);
    EXPECT_EQ(read->gridMode, "dynamic");
    ASSERT_EQ(read->gridCubes.size(), 1u);
    EXPECT_EQ(read->gridCubes[0].index, 12u);
    ExpectEqual(read->gridCubes[0].world, Matrix(1.0f));
    ASSERT_EQ(read->cubes.size(), 1u);
    EXPECT_EQ(read->cubes[0].name, "Orange cube");
    ExpectEqual(read->cubes[0].world, Matrix(2.0f));
    ASSERT_EQ(read->models.size(), 2u);
    EXPECT_EQ(read->models[0].path, (root.Path() / L"assets" / L"замок" / L"scene.gltf").lexically_normal());
    ExpectEqual(read->models[0].world, Matrix(3.0f));
    EXPECT_EQ(read->models[1].path, outside.lexically_normal());

    const std::string text = ReadText(file);
    EXPECT_NE(text.find(R"("assets/замок/scene.gltf")"), std::string::npos) << text;
    EXPECT_FALSE(fs::exists(fs::path(file) += L".tmp"));
}

TEST(SandboxSave, MissingFileIsFileNotFound) {
    const auto read = ReadSandboxSave(MissingFile(L"engine_test_save_missing.json"), {});

    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::FileNotFound);
}

TEST(SandboxSave, BrokenFileIsDecodeFailed) {
    const TempFile notJson(L"engine_test_save_broken.json", Bytes("{ \"version\": 1, "));
    const TempFile shortMatrix(L"engine_test_save_matrix.json",
                               Bytes(SaveJson("1", R"(, "cubes": [ { "name": "x", "world": [1, 2, 3] } ])")));

    for (const TempFile *file : {&notJson, &shortMatrix}) {
        const auto read = ReadSandboxSave(file->Path(), {});
        ASSERT_FALSE(read.has_value());
        EXPECT_EQ(read.error().code, ErrorCode::DecodeFailed) << read.error().message;
    }
}

TEST(SandboxSave, OtherVersionIsUnsupported) {
    const TempFile file(L"engine_test_save_version.json", Bytes(SaveJson("2")));

    const auto read = ReadSandboxSave(file.Path(), {});

    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::UnsupportedFormat);
}

TEST(SandboxSave, UnknownFieldsAreIgnoredAndSectionsAreOptional) {
    const TempFile file(L"engine_test_save_extra.json", Bytes(SaveJson("1", R"(, "lights": [1, 2], "camera": {})")));

    const auto read = ReadSandboxSave(file.Path(), {});

    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(read->gridCount, 0u);
    EXPECT_TRUE(read->gridCubes.empty());
    EXPECT_TRUE(read->cubes.empty());
    EXPECT_TRUE(read->models.empty());
}
