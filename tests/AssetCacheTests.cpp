#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "TestFiles.h"
#include "assets/AssetCache.h"
#include "renderdata/GpuUploader.h"

using engine::assets::AssetCache;
using engine::assets::TextureUsage;
using engine::core::ErrorCode;
using engine::render::MaterialHandle;
using engine::render::MeshData;
using engine::render::MeshHandle;
using engine::render::Model;
using engine::render::TextureData;
using engine::render::TextureHandle;
using engine::tests::MissingFile;
using engine::tests::TempFile;

namespace {
    // Stands in for the renderer: counts what the cache asks for and hands out increasing handles
    class FakeUploader final : public engine::render::GpuUploader {
    public:
        std::vector<MeshHandle> CreateMesh(const MeshData &mesh, std::wstring_view /*name*/) override {
            ++meshUploads;
            std::vector<MeshHandle> handles;
            for (size_t i = 0; i < mesh.submeshes.size(); ++i) {
                handles.push_back(MeshHandle{.index = m_nextMesh++});
            }
            return handles;
        }

        TextureHandle CreateTexture(const TextureData &data, std::wstring_view /*name*/) override {
            textures.push_back(data);
            return TextureHandle{.index = m_nextTexture++};
        }

        TextureHandle UnloadedTexture() const noexcept override { return TextureHandle{.index = kUnloaded}; }

        static constexpr uint32_t kUnloaded = 0;
        int meshUploads = 0;
        std::vector<TextureData> textures; // every texture created, in order

    private:
        uint32_t m_nextMesh = 0;
        uint32_t m_nextTexture = 1; // 0 is the unloaded texture
    };

    [[nodiscard]] std::span<const std::byte> Bytes(std::string_view text) {
        return std::as_bytes(std::span(text));
    }

    // Uncompressed 2x2 32-bit TGA, all white
    [[nodiscard]] std::vector<std::byte> Tga2x2() {
        std::vector<std::byte> file(18 + 2 * 2 * 4, std::byte{255});
        std::memset(file.data(), 0, 18);
        file[2] = std::byte{2};         // uncompressed true colour
        file[12] = std::byte{2};        // width
        file[14] = std::byte{2};        // height
        file[16] = std::byte{32};       // bits per pixel
        file[17] = std::byte{8 | 0x20}; // 8 alpha bits, top-left origin
        return file;
    }

    // Two objects of one triangle, each with its own material from engine_test_cache.mtl
    constexpr std::string_view kTwoMaterialsObj = "mtllib engine_test_cache.mtl\n"
                                                  "o first\n"
                                                  "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                                  "usemtl engine_test_first\n"
                                                  "f 1 2 3\n"
                                                  "o second\n"
                                                  "v 0 0 1\nv 1 0 1\nv 0 2 1\n"
                                                  "usemtl engine_test_second\n"
                                                  "f 4 5 6\n";

    [[nodiscard]] std::string Mtl(std::string_view first, std::string_view second) {
        return "newmtl engine_test_first\n" + std::string(first) + "\nnewmtl engine_test_second\n" +
               std::string(second) + "\n";
    }

    [[nodiscard]] const Model &Load(AssetCache &cache, const std::filesystem::path &path) {
        const auto model = cache.LoadModel(path);
        EXPECT_TRUE(model.has_value()) << model.error().message;
        return **model;
    }
} // namespace

// Also a second spelling of the same file: "." and another letter case
TEST(AssetCache, ImportsAModelOnce) {
    const TempFile obj(L"engine_test_cache_once.obj", Bytes("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &first = Load(cache, obj.Path());
    const Model &second = Load(cache, obj.Path().parent_path() / "." / L"ENGINE_TEST_CACHE_ONCE.OBJ");

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(gpu.meshUploads, 1);
    EXPECT_EQ(cache.ModelCount(), 1u);
}

TEST(AssetCache, MissingModelIsAnErrorAndNotRemembered) {
    FakeUploader gpu;
    AssetCache cache(gpu);

    const auto model = cache.LoadModel(MissingFile(L"engine_test_cache_missing.obj"));

    ASSERT_FALSE(model.has_value());
    EXPECT_EQ(model.error().code, ErrorCode::FileNotFound);
    EXPECT_EQ(cache.ModelCount(), 0u);
    EXPECT_EQ(gpu.meshUploads, 0);
}

// One part per placed submesh, with its mesh and place; the bounds cover both triangles
TEST(AssetCache, ModelHasAPartPerSubmeshAndItsBounds) {
    const TempFile mtl(L"engine_test_cache.mtl", Bytes(Mtl("Kd 1 1 1", "Kd 1 1 1")));
    const TempFile obj(L"engine_test_cache_parts.obj", Bytes(kTwoMaterialsObj));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, obj.Path());

    ASSERT_EQ(model.parts.size(), 2u);
    EXPECT_NE(model.parts[0].mesh, model.parts[1].mesh);
    EXPECT_NEAR(model.bounds.min.x, 0.0f, 1e-5f);
    EXPECT_NEAR(model.bounds.max.x, 1.0f, 1e-5f);
    EXPECT_NEAR(model.bounds.max.y, 2.0f, 1e-5f);
}

// Untextured materials: colour as is (no sRGB conversion), one shared texture per colour. Assimp's unused
// DefaultMaterial gets none.
TEST(AssetCache, MaterialWithoutTextureGetsItsColour) {
    const TempFile mtl(L"engine_test_cache.mtl", Bytes(Mtl("Kd 1 0.5 0", "Kd 1 0.5 0")));
    const TempFile obj(L"engine_test_cache_colour.obj", Bytes(kTwoMaterialsObj));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, obj.Path());

    ASSERT_EQ(gpu.textures.size(), 1u);
    const TextureData &colour = gpu.textures[0];
    EXPECT_TRUE(colour.isSrgb);
    ASSERT_EQ(colour.mips.size(), 1u);
    EXPECT_EQ(colour.mips[0].width, 1u);
    EXPECT_EQ(colour.pixels[0], std::byte{255});
    EXPECT_EQ(colour.pixels[1], std::byte{128});
    EXPECT_EQ(colour.pixels[2], std::byte{0});
    ASSERT_EQ(model.parts.size(), 2u);
    EXPECT_EQ(model.parts[0].material, (MaterialHandle{.index = 1}));
    EXPECT_EQ(model.parts[1].material, (MaterialHandle{.index = 1}));
}

TEST(AssetCache, BlackMaterialShowsTheGreyTexture) {
    const TempFile mtl(L"engine_test_cache.mtl", Bytes(Mtl("Kd 0 0 0", "Kd 0 0 0")));
    const TempFile obj(L"engine_test_cache_black.obj", Bytes(kTwoMaterialsObj));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, obj.Path());

    EXPECT_TRUE(gpu.textures.empty());
    ASSERT_FALSE(model.parts.empty());
    EXPECT_EQ(model.parts[0].material, (MaterialHandle{.index = FakeUploader::kUnloaded}));
}

// Two materials with the same albedo file load it once; a file that is missing becomes the grey texture
TEST(AssetCache, TextureFilesLoadOnceAndMissingOnesAreGrey) {
    const TempFile tga(L"engine_test_cache_albedo.tga", Tga2x2());
    const TempFile mtl(L"engine_test_cache.mtl",
                       Bytes(Mtl("map_Kd engine_test_cache_albedo.tga", "map_Kd engine_test_cache_albedo.tga")));
    const TempFile obj(L"engine_test_cache_files.obj", Bytes(kTwoMaterialsObj));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, obj.Path());

    EXPECT_EQ(gpu.textures.size(), 1u);
    ASSERT_EQ(model.parts.size(), 2u);
    EXPECT_EQ(model.parts[0].material, model.parts[1].material);
    EXPECT_NE(model.parts[0].material, (MaterialHandle{.index = FakeUploader::kUnloaded}));
    // The same file asked for directly: no new texture
    EXPECT_EQ(cache.LoadTexture(tga.Path(), TextureUsage::Diffuse).index, model.parts[0].material.index);
    EXPECT_EQ(gpu.textures.size(), 1u);

    EXPECT_EQ(cache.LoadTexture(MissingFile(L"engine_test_cache_none.png"), TextureUsage::Diffuse).index,
              FakeUploader::kUnloaded);
    EXPECT_EQ(gpu.textures.size(), 1u);
}
