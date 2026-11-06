#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
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
    [[nodiscard]] std::span<const std::byte> Bytes(std::string_view text) {
        return std::as_bytes(std::span(text));
    }

    // One triangle in the XY plane
    constexpr std::string_view kTriangleObj = "v 0 0 0\n"
                                              "v 1 0 0\n"
                                              "v 0 1 0\n"
                                              "f 1 2 3\n";

    // Two objects of one triangle each, the second at z = 1: Assimp makes one mesh per object
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

    // A .gltf whose mesh 0 is one triangle. The buffer holds the positions (0,0,0), (1,0,0), (0,1,0) as floats,
    // then the indices 0, 1, 2 as uint16 and 2 bytes of padding. `scenesAndNodes` places the mesh.
    [[nodiscard]] std::string TriangleGltf(std::string_view scenesAndNodes) {
        return std::format(R"({{
  "asset": {{"version": "2.0"}},
  "buffers": [{{"byteLength": 44,
               "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}}],
  "bufferViews": [{{"buffer": 0, "byteOffset": 0, "byteLength": 36}},
                  {{"buffer": 0, "byteOffset": 36, "byteLength": 6}}],
  "accessors": [{{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                 "min": [0, 0, 0], "max": [1, 1, 0]}},
                {{"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}}],
  "meshes": [{{"primitives": [{{"attributes": {{"POSITION": 0}}, "indices": 1}}]}}],
  {}
}})",
                           scenesAndNodes);
    }
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
        // The file has no normals: GenSmoothNormals makes them. The triangle lies in the XY plane.
        EXPECT_NEAR(std::abs(vertex.normal.z), 1.0f, 1e-5f);
        // No UVs, so no tangents from Assimp: +X, as in DX11
        EXPECT_EQ(vertex.tangent.x, 1.0f);
        EXPECT_EQ(vertex.tangent.y, 0.0f);
        EXPECT_EQ(vertex.tangent.z, 0.0f);
    }
}

// Every submesh is a range of the same two arrays; its indices count from its own first vertex
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
        // The second object is the one at z = 1 (-1 after the left-handed conversion)
        EXPECT_NEAR(std::abs(mesh.vertices[second.baseVertex + index].position.z), 1.0f, 1e-5f);
    }
    ASSERT_EQ(result->nodes.size(), 2u);
    EXPECT_NE(result->nodes[0].submesh, result->nodes[1].submesh);
}

// The renderer draws triangles only; a submesh keeps its place (submesh i = Assimp mesh i) even if it is empty
TEST(ImportModel, PointsAndLinesAreSkipped) {
    const TempFile file(L"engine_test_lines.obj", Bytes(std::string(kTriangleObj) + "l 1 2\n"
                                                                                    "p 3\n"));

    const auto result = ImportModel(file.Path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->mesh.indices.size(), 3u);
}

// A mesh used by two nodes is one submesh placed twice. A child is placed in its parent's space: the parent turns
// 90 degrees around Z, so the child's offset (1, 0, 0) ends up at (0, 1, 0). The other way round it would stay.
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
    // Row-vector convention: the translation is in row 3
    const auto &child = result->nodes[0].transform;
    EXPECT_NEAR(child(3, 0), 0.0f, 1e-5f);
    EXPECT_NEAR(child(3, 1), 1.0f, 1e-5f);
    EXPECT_NEAR(child(3, 2), 0.0f, 1e-5f);
    const auto &other = result->nodes[1].transform;
    EXPECT_NEAR(other(3, 0), 5.0f, 1e-5f);
    EXPECT_NEAR(other(3, 1), 0.0f, 1e-5f);
}
