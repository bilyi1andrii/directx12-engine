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
using engine::render::AlphaMode;
using engine::render::MaterialHandle;
using engine::render::MaterialInfo;
using engine::render::MeshData;
using engine::render::MeshHandle;
using engine::render::Model;
using engine::render::Shading;
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

        MaterialHandle CreateMaterial(const MaterialInfo &material) override {
            materials.push_back(material);
            return MaterialHandle{.index = static_cast<uint32_t>(materials.size() - 1)};
        }

        // What CreateMaterial was given for `handle`
        [[nodiscard]] const MaterialInfo &Material(MaterialHandle handle) const { return materials.at(handle.index); }

        static constexpr uint32_t kUnloaded = 0;
        int meshUploads = 0;
        std::vector<TextureData> textures; // every texture created, in order
        // Every material created, in order. Like the renderer, material 0 exists from the start.
        std::vector<MaterialInfo> materials{MaterialInfo{}};

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

    // One triangle whose material's albedo is a 2x2 PNG stored in the file (a data URI): an embedded texture
    constexpr std::string_view kEmbeddedTextureGltf = R"({
  "asset": {"version": "2.0"},
  "buffers": [{"byteLength": 44,
               "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}, {"buffer": 0, "byteOffset": 36, "byteLength": 6}],
  "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]},
                {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "material": 0}]}],
  "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
  "textures": [{"source": 0}],
  "images": [{"uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAARSURBVBhXYziRYvQfhBlgDABUQAl1/dX3fwAAAABJRU5ErkJggg=="}],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0}]
})";

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

TEST(AssetCache, CubeModelsShareOneMeshAndUseTheirTexture) {
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &first = cache.CubeModel(TextureHandle{.index = 7});
    const Model &second = cache.CubeModel(TextureHandle{.index = 8});
    const Model &firstAgain = cache.CubeModel(TextureHandle{.index = 7});

    EXPECT_EQ(&first, &firstAgain);
    EXPECT_EQ(gpu.meshUploads, 1);
    ASSERT_EQ(first.parts.size(), 1u);
    ASSERT_EQ(second.parts.size(), 1u);
    EXPECT_EQ(first.parts[0].mesh, second.parts[0].mesh);
    EXPECT_EQ(gpu.Material(first.parts[0].material), MaterialInfo{.albedo = TextureHandle{.index = 7}});
    EXPECT_EQ(gpu.Material(second.parts[0].material), MaterialInfo{.albedo = TextureHandle{.index = 8}});
    EXPECT_EQ(first.bounds.min.x, -1.0f);
    EXPECT_EQ(first.bounds.max.y, 1.0f);
    EXPECT_EQ(first.triangleCount, 12u);
}

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
    EXPECT_EQ(model.triangleCount, 2u);
    EXPECT_NEAR(model.bounds.min.x, 0.0f, 1e-5f);
    EXPECT_NEAR(model.bounds.max.x, 1.0f, 1e-5f);
    EXPECT_NEAR(model.bounds.max.y, 2.0f, 1e-5f);
    // Each part's box covers its own triangle only
    EXPECT_NEAR(model.parts[0].bounds.max.y, 1.0f, 1e-5f);
    EXPECT_NEAR(model.parts[1].bounds.max.y, 2.0f, 1e-5f);
}

// A part's box must not include the node transform: picking applies that separately
TEST(AssetCache, PartBoundsAreInTheSubmeshsOwnSpace) {
    std::string text(kEmbeddedTextureGltf);
    const std::string_view node = R"({"mesh": 0})";
    text.replace(text.find(node), node.size(), R"({"mesh": 0, "translation": [5, 0, 0]})");
    const TempFile gltf(L"engine_test_cache_node.gltf", Bytes(text));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, gltf.Path());

    ASSERT_EQ(model.parts.size(), 1u);
    EXPECT_NEAR(model.parts[0].bounds.max.x, 1.0f, 1e-5f);
    EXPECT_NEAR(model.bounds.max.x, 6.0f, 1e-5f);
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
    EXPECT_EQ(model.parts[0].material, model.parts[1].material);
    EXPECT_EQ(gpu.Material(model.parts[0].material).albedo, TextureHandle{.index = 1});
}

TEST(AssetCache, BlackMaterialShowsTheGreyTexture) {
    const TempFile mtl(L"engine_test_cache.mtl", Bytes(Mtl("Kd 0 0 0", "Kd 0 0 0")));
    const TempFile obj(L"engine_test_cache_black.obj", Bytes(kTwoMaterialsObj));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, obj.Path());

    EXPECT_TRUE(gpu.textures.empty());
    ASSERT_FALSE(model.parts.empty());
    EXPECT_EQ(gpu.Material(model.parts[0].material).albedo.index, FakeUploader::kUnloaded);
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
    const TextureHandle albedo = gpu.Material(model.parts[0].material).albedo;
    EXPECT_NE(albedo.index, FakeUploader::kUnloaded);
    // The same file asked for directly: no new texture
    EXPECT_EQ(cache.LoadTexture(tga.Path(), TextureUsage::Diffuse), albedo);
    EXPECT_EQ(gpu.textures.size(), 1u);

    EXPECT_EQ(cache.LoadTexture(MissingFile(L"engine_test_cache_none.png"), TextureUsage::Diffuse).index,
              FakeUploader::kUnloaded);
    EXPECT_EQ(gpu.textures.size(), 1u);
    // The grey texture of the missing file was not created by the cache
    EXPECT_EQ(cache.TextureCount(), 1u);
}

// TextureCount() (the "new textures" of the load log) includes images stored in the model file
TEST(AssetCache, TextureCountIncludesEmbeddedImages) {
    const TempFile gltf(L"engine_test_cache_embedded.gltf", Bytes(kEmbeddedTextureGltf));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, gltf.Path());

    ASSERT_EQ(gpu.textures.size(), 1u);
    EXPECT_EQ(cache.TextureCount(), 1u);
    ASSERT_EQ(model.parts.size(), 1u);
    EXPECT_NE(gpu.Material(model.parts[0].material).albedo.index, FakeUploader::kUnloaded);
}

TEST(AssetCache, EqualMaterialsShareAHandle) {
    FakeUploader gpu;
    AssetCache cache(gpu);
    const TextureHandle texture{.index = 7};

    const MaterialHandle lit = cache.Material({.albedo = texture});
    const MaterialHandle unlit = cache.Material({.albedo = texture, .shading = Shading::Unlit});
    const MaterialHandle twoSided = cache.Material({.albedo = texture, .doubleSided = true});
    const MaterialHandle otherTexture = cache.Material({.albedo = TextureHandle{.index = 8}});
    const MaterialInfo mask{.albedo = texture, .alphaMode = AlphaMode::Mask, .opacityMap = TextureHandle{.index = 9}};
    MaterialInfo otherCutoff = mask;
    otherCutoff.alphaCutoff = 77;
    const MaterialHandle masked = cache.Material(mask);
    const MaterialHandle maskedOtherCutoff = cache.Material(otherCutoff);
    MaterialInfo blend = mask;
    blend.alphaMode = AlphaMode::Blend;
    blend.opacity = 64;
    blend.alphaCutoff = 77;
    const MaterialHandle blended = cache.Material(blend);

    EXPECT_EQ(cache.Material({.albedo = texture}), lit);
    EXPECT_EQ(cache.Material({.albedo = texture, .shading = Shading::Unlit}), unlit);
    EXPECT_EQ(cache.Material({.albedo = texture, .doubleSided = true}), twoSided);
    EXPECT_EQ(cache.Material(mask), masked);
    // Each mode ignores the alpha value it does not use: a cutoff for Blend, an opacity for Mask and Opaque
    MaterialInfo blendOtherCutoff = blend;
    blendOtherCutoff.alphaCutoff = 200;
    EXPECT_EQ(cache.Material(blendOtherCutoff), blended);
    EXPECT_EQ(cache.Material(MaterialInfo{.albedo = texture, .opacity = 10}), lit);
    // Seven different materials, each created once (material 0 was there before)
    EXPECT_EQ(gpu.materials.size(), 8u);
    EXPECT_NE(lit, unlit);
    EXPECT_NE(lit, twoSided);
    EXPECT_NE(lit, otherTexture);
    EXPECT_NE(lit, masked);
    EXPECT_NE(masked, maskedOtherCutoff);
    EXPECT_EQ(gpu.Material(unlit), (MaterialInfo{.albedo = texture, .shading = Shading::Unlit}));
    EXPECT_EQ(gpu.Material(twoSided), (MaterialInfo{.albedo = texture, .doubleSided = true}));
}

TEST(AssetCache, MaskMaterialGetsItsOpacityMapOrWhite) {
    const TempFile albedo(L"engine_test_cache_leaf.tga", Tga2x2());
    const TempFile mask(L"engine_test_cache_leaf_mask.tga", Tga2x2());
    const TempFile mtl(L"engine_test_cache.mtl", Bytes(Mtl("map_Kd engine_test_cache_leaf.tga\n"
                                                           "map_d engine_test_cache_leaf_mask.tga",
                                                           "map_Kd engine_test_cache_leaf.tga\n"
                                                           "map_d engine_test_cache_none_mask.png")));
    const TempFile obj(L"engine_test_cache_mask.obj", Bytes(kTwoMaterialsObj));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &model = Load(cache, obj.Path());

    ASSERT_EQ(model.parts.size(), 2u);
    const MaterialInfo &leaf = gpu.Material(model.parts[0].material);
    EXPECT_EQ(leaf.alphaMode, AlphaMode::Mask);
    EXPECT_EQ(leaf.alphaCutoff, 128);
    EXPECT_NE(leaf.opacityMap, leaf.albedo);
    ASSERT_NE(leaf.opacityMap.index, FakeUploader::kUnloaded);
    EXPECT_TRUE(gpu.textures[leaf.albedo.index - 1].isSrgb);
    EXPECT_FALSE(gpu.textures[leaf.opacityMap.index - 1].isSrgb) << "a mask is data, not colour";

    const MaterialInfo &broken = gpu.Material(model.parts[1].material);
    EXPECT_EQ(broken.alphaMode, AlphaMode::Mask);
    const size_t textures = gpu.textures.size();
    EXPECT_EQ(broken.opacityMap, cache.SolidColor(255, 255, 255));
    EXPECT_EQ(gpu.textures.size(), textures) << "the white texture was made for the material";
}

TEST(AssetCache, ModelMaterialsPassDoubleSidedOn) {
    std::string text(kEmbeddedTextureGltf);
    const std::string_view material = R"("materials": [{)";
    text.replace(text.find(material), material.size(), R"("materials": [{"doubleSided": true, )");
    const TempFile twoSidedGltf(L"engine_test_cache_two_sided.gltf", Bytes(text));
    const TempFile oneSidedGltf(L"engine_test_cache_one_sided.gltf", Bytes(kEmbeddedTextureGltf));
    FakeUploader gpu;
    AssetCache cache(gpu);

    const Model &twoSided = Load(cache, twoSidedGltf.Path());
    const Model &oneSided = Load(cache, oneSidedGltf.Path());

    ASSERT_EQ(twoSided.parts.size(), 1u);
    ASSERT_EQ(oneSided.parts.size(), 1u);
    EXPECT_TRUE(gpu.Material(twoSided.parts[0].material).doubleSided);
    EXPECT_FALSE(gpu.Material(oneSided.parts[0].material).doubleSided);
    EXPECT_EQ(gpu.Material(twoSided.parts[0].material).shading, Shading::Lit);
}
