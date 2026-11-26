// Tests reading model files: geometry, node placement, materials and the errors for broken files.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include <gtest/gtest.h>

#include "TestFiles.h"
#include "assets/ModelImporter.h"
#include "core/Utf8.h"

using assets::ImportModel;
using assets::IsModelFile;
using core::ErrorCode;
using render::AlphaMode;
using render::EmbeddedTexture;
using render::MaterialDesc;
using render::ModelData;
using render::TextureSource;
using tests::MissingFile;
using tests::TempFile;

namespace {
    [[nodiscard]] std::span<const std::byte> Bytes(std::string_view text) {
        return std::as_bytes(std::span(text));
    }

    constexpr std::string_view kTriangleObj = "v 0 0 0\n"
                                              "v 1 0 0\n"
                                              "v 0 1 0\n"
                                              "f 1 2 3\n";

    constexpr std::string_view kTwoObjectsObj = "o first\n"
                                                "v 0 0 0\n"
                                                "v 1 0 0\n"
                                                "v 0 1 0\n"
                                                "f 1 2 3\n"
                                                "o second\n"
                                                "v 0 0 1\n"
                                                "v 1 0 1\n"
                                                "v 0 1 1\n"
                                                "f 4 5 6\n";

    [[nodiscard]] std::string TriangleGltf(std::string_view rest, bool withMaterial = false) {
        return std::format(R"({{
  "asset": {{"version": "2.0"}},
  "buffers": [{{"byteLength": 44,
               "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}}],
  "bufferViews": [{{"buffer": 0, "byteOffset": 0, "byteLength": 36}},
                  {{"buffer": 0, "byteOffset": 36, "byteLength": 6}}],
  "accessors": [{{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                 "min": [0, 0, 0], "max": [1, 1, 0]}},
                {{"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}}],
  "meshes": [{{"primitives": [{{"attributes": {{"POSITION": 0}}, "indices": 1{}}}]}}],
  {}
}})",
                           withMaterial ? R"(, "material": 0)" : "", rest);
    }

    [[nodiscard]] std::wstring FileOf(const TextureSource &texture) {
        const auto *path = std::get_if<std::filesystem::path>(&texture);
        return path != nullptr ? path->native() : std::wstring{};
    }

    [[nodiscard]] int EmbeddedIndexOf(const TextureSource &texture) {
        const auto *embedded = std::get_if<EmbeddedTexture>(&texture);
        return embedded != nullptr ? static_cast<int>(embedded->index) : -1;
    }

    [[nodiscard]] std::wstring Normalized(const std::filesystem::path &file) {
        return std::filesystem::weakly_canonical(file).native();
    }

    [[nodiscard]] const MaterialDesc &MaterialOfFirstSubmesh(const ModelData &model) {
        return model.materials.at(model.mesh.submeshes.at(0).materialIndex);
    }

    constexpr std::span<const std::byte> kEmpty;

    [[nodiscard]] MaterialDesc ImportLeafMaterial(std::string_view mtl) {
        const TempFile mtlFile(L"engine_test_leaf.mtl",
                               Bytes(std::string("newmtl engine_test_leaf\n") + std::string(mtl)));
        const TempFile obj(L"engine_test_leaf.obj", Bytes(std::string("mtllib engine_test_leaf.mtl\n"
                                                                      "usemtl engine_test_leaf\n") +
                                                          std::string(kTriangleObj)));
        const auto result = ImportModel(obj.Path());
        if (!result) {
            ADD_FAILURE() << result.error().message;
            return {};
        }
        return MaterialOfFirstSubmesh(*result);
    }
} // namespace

TEST(ImportModel, MissingFileReturnsFileNotFound) {
    const auto path = MissingFile(L"engine_test_missing.obj");

    const auto result = ImportModel(path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::FileNotFound) << result.error().message;
    EXPECT_NE(result.error().message.find(core::ToUtf8(path.filename().native())), std::string::npos)
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

TEST(ImportModel, FaceIndexOutsideTheVerticesReturnsDecodeFailed) {
    const TempFile file(L"engine_test_bad_index.ply", Bytes("ply\n"
                                                            "format ascii 1.0\n"
                                                            "element vertex 3\n"
                                                            "property float x\n"
                                                            "property float y\n"
                                                            "property float z\n"
                                                            "element face 1\n"
                                                            "property list uchar int vertex_indices\n"
                                                            "end_header\n"
                                                            "0 0 0\n"
                                                            "1 0 0\n"
                                                            "0 1 0\n"
                                                            "3 0 1 99\n"));

    const auto result = ImportModel(file.Path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DecodeFailed) << result.error().message;
}

TEST(ImportModel, ModelWithoutTrianglesReturnsDecodeFailed) {
    const TempFile file(L"engine_test_lines.obj", Bytes("v 0 0 0\n"
                                                        "v 1 0 0\n"
                                                        "v 0 1 0\n"
                                                        "l 1 2\n"
                                                        "p 3\n"));

    const auto result = ImportModel(file.Path());

    ASSERT_FALSE(result.has_value()) << result->mesh.vertices.size() << " vertices, " << result->mesh.indices.size()
                                     << " indices";
    EXPECT_EQ(result.error().code, ErrorCode::DecodeFailed) << result.error().message;
}

TEST(ImportModel, ReadsObj) {
    const TempFile file(L"engine_test_triangle.obj", Bytes(kTriangleObj));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    const auto &mesh = result->mesh;
    ASSERT_EQ(mesh.vertices.size(), 3u);
    EXPECT_EQ(mesh.indices.size(), 3u);
    ASSERT_EQ(mesh.submeshes.size(), 1u);
    EXPECT_EQ(mesh.submeshes[0].firstIndex, 0u);
    EXPECT_EQ(mesh.submeshes[0].indexCount, 3u);
    EXPECT_EQ(mesh.submeshes[0].baseVertex, 0);
    EXPECT_LT(mesh.submeshes[0].materialIndex, result->materials.size());
    ASSERT_EQ(result->nodes.size(), 1u);
    EXPECT_EQ(result->nodes[0].submesh, 0u);
    for (const auto &vertex : mesh.vertices) {
        EXPECT_NEAR(std::abs(vertex.normal.z), 1.0f, 1e-5f);
        EXPECT_EQ(vertex.tangent.x, 1.0f);
        EXPECT_EQ(vertex.tangent.y, 0.0f);
        EXPECT_EQ(vertex.tangent.z, 0.0f);
    }
}

TEST(ImportModel, SubmeshesShareOneVertexAndIndexArray) {
    const TempFile file(L"engine_test_two_objects.obj", Bytes(kTwoObjectsObj));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    const auto &mesh = result->mesh;
    ASSERT_EQ(mesh.submeshes.size(), 2u);
    ASSERT_EQ(mesh.vertices.size(), 6u);
    ASSERT_EQ(mesh.indices.size(), 6u);
    const auto &second = mesh.submeshes[1];
    EXPECT_EQ(second.firstIndex, 3u);
    EXPECT_EQ(second.indexCount, 3u);
    EXPECT_EQ(second.baseVertex, 3);
    for (uint32_t i = 0; i < second.indexCount; ++i) {
        const uint32_t index = mesh.indices[second.firstIndex + i];
        EXPECT_LT(index, 3u) << "indices are relative to baseVertex";
        EXPECT_NEAR(std::abs(mesh.vertices[second.baseVertex + index].position.z), 1.0f, 1e-5f);
    }
    ASSERT_EQ(result->nodes.size(), 2u);
    EXPECT_NE(result->nodes[0].submesh, result->nodes[1].submesh);
}

TEST(ImportModel, PointsAndLinesAreSkipped) {
    const TempFile file(L"engine_test_lines.obj", Bytes(std::string(kTriangleObj) + "l 1 2\n"
                                                                                    "p 3\n"));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->mesh.indices.size(), 3u);
}

TEST(ImportModel, NodeTransformsIncludeTheirParents) {
    const std::string gltf = TriangleGltf(R"("scene": 0,
  "scenes": [{"nodes": [0, 2]}],
  "nodes": [{"rotation": [0, 0, 0.70710678, 0.70710678], "children": [1]},
            {"translation": [1, 0, 0], "mesh": 0},
            {"translation": [5, 0, 0], "mesh": 0}])");
    const TempFile file(L"engine_test_nodes.gltf", Bytes(gltf));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result->mesh.submeshes.size(), 1u);
    ASSERT_EQ(result->nodes.size(), 2u);
    EXPECT_EQ(result->nodes[0].submesh, 0u);
    EXPECT_EQ(result->nodes[1].submesh, 0u);
    const auto &child = result->nodes[0].transform;
    EXPECT_NEAR(child(3, 0), 0.0f, 1e-5f);
    EXPECT_NEAR(child(3, 1), 1.0f, 1e-5f);
    EXPECT_NEAR(child(3, 2), 0.0f, 1e-5f);
    const auto &other = result->nodes[1].transform;
    EXPECT_NEAR(other(3, 0), 5.0f, 1e-5f);
    EXPECT_NEAR(other(3, 1), 0.0f, 1e-5f);
}

TEST(ImportModel, ObjMaterialFollowsTheNonPbrRules) {
    const TempFile mtl(L"engine_test_mat.mtl", Bytes("newmtl engine_test_brick\n"
                                                     "Kd 0.5 0.25 1\n"
                                                     "d 0.5\n"
                                                     "map_Kd ENGINE_TEST_BRICK_ALBEDO.PNG\n"));
    const TempFile albedo(L"engine_test_brick_albedo.png", kEmpty);
    const TempFile normal(L"engine_test_brick_n.png", kEmpty);
    const TempFile occlusion(L"engine_test_brick_ao.jpg", kEmpty);
    const TempFile obj(
        L"engine_test_mat.obj",
        Bytes(std::string("mtllib engine_test_mat.mtl\nusemtl engine_test_brick\n") + std::string(kTriangleObj)));

    const auto result = ImportModel(obj.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    const MaterialDesc &material = MaterialOfFirstSubmesh(*result);
    EXPECT_EQ(material.name, "engine_test_brick");
    EXPECT_EQ(FileOf(material.albedo), Normalized(albedo.Path()));
    EXPECT_EQ(material.baseColor.x, 1.0f) << "a texture is not tinted by the diffuse colour";
    EXPECT_EQ(material.baseColor.y, 1.0f);
    EXPECT_EQ(material.baseColor.z, 1.0f);
    EXPECT_FLOAT_EQ(material.baseColor.w, 0.5f);
    EXPECT_EQ(material.alphaMode, AlphaMode::Blend);
    EXPECT_EQ(FileOf(material.normal), Normalized(normal.Path()));
    EXPECT_EQ(FileOf(material.ambientOcclusion), Normalized(occlusion.Path()));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(material.metallic)) << "no engine_test_brick_m file";
    EXPECT_FALSE(material.packedMetallicRoughness);
    EXPECT_TRUE(result->embeddedTextures.empty());
}

TEST(ImportModel, ObjAlphaModeFollowsOpacityAndOpacityMap) {
    struct Case {
        std::string_view mtl;
        std::wstring_view fileByName;
        AlphaMode alphaMode;
        std::wstring_view opacityMap;
        bool doubleSided;
    };
    constexpr Case kCases[] = {
        {"map_Kd engine_test_leaf_albedo.png\n", L"", AlphaMode::Opaque, L"", false},
        {"map_Kd engine_test_leaf_albedo.png\nmap_d engine_test_leaf_mask.png\n", L"", AlphaMode::Mask,
         L"engine_test_leaf_mask.png", false},
        {"d 0.5\nmap_Kd engine_test_leaf_albedo.png\nmap_d engine_test_leaf_mask.png\n", L"", AlphaMode::Blend,
         L"engine_test_leaf_mask.png", true},
        {"map_Kd engine_test_leaf_albedo.png\n", L"engine_test_leaf_d.png", AlphaMode::Mask, L"engine_test_leaf_d.png",
         false},
    };
    for (const Case &test : kCases) {
        std::optional<TempFile> byName;
        if (!test.fileByName.empty()) {
            byName.emplace(test.fileByName, kEmpty);
        }

        const MaterialDesc material = ImportLeafMaterial(test.mtl);

        EXPECT_EQ(material.alphaMode, test.alphaMode) << test.mtl;
        EXPECT_FLOAT_EQ(material.alphaCutoff, 0.5f) << test.mtl;
        EXPECT_EQ(material.doubleSided, test.doubleSided) << test.mtl;
        EXPECT_EQ(FileOf(material.opacityMap),
                  test.opacityMap.empty() ? std::wstring{} : Normalized(MissingFile(test.opacityMap)))
            << test.mtl;
    }
}

TEST(ImportModel, ObjOpacityMapThatIsTheAlbedoMakesMaskWithoutSeparateMap) {
    const TempFile albedo(L"engine_test_leaf_albedo.png", kEmpty);

    const MaterialDesc material = ImportLeafMaterial("map_Kd engine_test_leaf_albedo.png\n"
                                                     "map_d ENGINE_TEST_LEAF_ALBEDO.PNG\n");

    EXPECT_EQ(material.alphaMode, AlphaMode::Mask);
    EXPECT_FLOAT_EQ(material.alphaCutoff, 0.5f);
    EXPECT_EQ(FileOf(material.albedo), Normalized(albedo.Path()));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(material.opacityMap));
}

TEST(ImportModel, GltfMaterialKeepsTheFileValues) {
    const std::string gltf = TriangleGltf(R"("scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0}],
  "materials": [{"name": "leaf", "alphaMode": "MASK", "alphaCutoff": 0.3, "doubleSided": true,
                 "emissiveFactor": [0.1, 0.2, 0.3],
                 "pbrMetallicRoughness": {"baseColorFactor": [1, 0.5, 0.25, 0.75],
                                          "metallicFactor": 0.2, "roughnessFactor": 0.7,
                                          "baseColorTexture": {"index": 0},
                                          "metallicRoughnessTexture": {"index": 1}},
                 "occlusionTexture": {"index": 1}}],
  "textures": [{"source": 0}, {"source": 1}],
  "images": [{"uri": "engine_test_leaf.png"}, {"uri": "data:image/png;base64,AQIDBA=="}])",
                                          true);
    const TempFile file(L"engine_test_material.gltf", Bytes(gltf));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    const MaterialDesc &material = MaterialOfFirstSubmesh(*result);
    EXPECT_EQ(material.name, "leaf");
    EXPECT_EQ(material.alphaMode, AlphaMode::Mask);
    EXPECT_FLOAT_EQ(material.alphaCutoff, 0.3f);
    EXPECT_TRUE(material.doubleSided);
    EXPECT_FLOAT_EQ(material.baseColor.x, 1.0f);
    EXPECT_FLOAT_EQ(material.baseColor.y, 0.5f);
    EXPECT_FLOAT_EQ(material.baseColor.z, 0.25f);
    EXPECT_FLOAT_EQ(material.baseColor.w, 0.75f);
    EXPECT_FLOAT_EQ(material.metallicFactor, 0.2f);
    EXPECT_FLOAT_EQ(material.roughnessFactor, 0.7f);
    EXPECT_FLOAT_EQ(material.emissiveFactor.x, 0.1f);
    EXPECT_FLOAT_EQ(material.emissiveFactor.y, 0.2f);
    EXPECT_FLOAT_EQ(material.emissiveFactor.z, 0.3f);
    EXPECT_EQ(FileOf(material.albedo), Normalized(file.Path().parent_path() / "engine_test_leaf.png"));

    EXPECT_TRUE(material.packedMetallicRoughness);
    EXPECT_EQ(EmbeddedIndexOf(material.metallic), 0);
    EXPECT_EQ(EmbeddedIndexOf(material.roughness), 0);
    EXPECT_EQ(EmbeddedIndexOf(material.ambientOcclusion), 0);
    ASSERT_EQ(result->embeddedTextures.size(), 1u);
    const auto &image = result->embeddedTextures[0];
    EXPECT_EQ(image.formatHint, "png");
    ASSERT_EQ(image.bytes.size(), 4u);
    EXPECT_EQ(image.bytes[0], std::byte{1});
    EXPECT_EQ(image.bytes[3], std::byte{4});
}

TEST(IsModelFile, KnowsTheModelFormatsInAnyLetterCase) {
    EXPECT_TRUE(IsModelFile("town/scene.gltf"));
    EXPECT_TRUE(IsModelFile("town/Scene.GLB"));
    EXPECT_TRUE(IsModelFile("cube.obj"));
    EXPECT_TRUE(IsModelFile("C:/models/Rig.Fbx"));
    EXPECT_FALSE(IsModelFile("texture.png"));
    EXPECT_FALSE(IsModelFile("scene.gltf.bak"));
    EXPECT_FALSE(IsModelFile("gltf"));
    EXPECT_FALSE(IsModelFile("folder/"));
}
