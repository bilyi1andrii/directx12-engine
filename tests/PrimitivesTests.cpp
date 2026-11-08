#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <tuple>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "assets/Primitives.h"
#include "renderdata/MeshData.h"

using DirectX::XMFLOAT3;
using engine::assets::MakeCube;
using engine::render::MeshData;
using engine::render::Vertex;

namespace {
    constexpr float kTolerance = 1e-5f;

    [[nodiscard]] XMFLOAT3 Sub(const XMFLOAT3 &a, const XMFLOAT3 &b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    [[nodiscard]] float Dot(const XMFLOAT3 &a, const XMFLOAT3 &b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    [[nodiscard]] XMFLOAT3 Cross(const XMFLOAT3 &a, const XMFLOAT3 &b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    [[nodiscard]] float Length(const XMFLOAT3 &v) {
        return std::sqrt(Dot(v, v));
    }
} // namespace

TEST(MakeCube, HasOneSubmeshCoveringFourVerticesAndTwoTrianglesPerFace) {
    const MeshData cube = MakeCube();

    EXPECT_EQ(cube.vertices.size(), 24u);
    ASSERT_EQ(cube.indices.size(), 36u);
    ASSERT_EQ(cube.submeshes.size(), 1u);
    EXPECT_EQ(cube.submeshes[0].firstIndex, 0u);
    EXPECT_EQ(cube.submeshes[0].indexCount, 36u);
    EXPECT_EQ(cube.submeshes[0].baseVertex, 0);
    EXPECT_EQ(cube.submeshes[0].materialIndex, 0u);
    for (const uint32_t index : cube.indices) {
        EXPECT_LT(index, cube.vertices.size());
    }
}

TEST(MakeCube, CornersAreAtPlusMinusOneOnTheirFaceWithUnitOutwardNormals) {
    const MeshData cube = MakeCube();

    std::set<std::tuple<float, float, float>> faceNormals;
    for (const Vertex &vertex : cube.vertices) {
        EXPECT_NEAR(std::abs(vertex.position.x), 1.0f, kTolerance);
        EXPECT_NEAR(std::abs(vertex.position.y), 1.0f, kTolerance);
        EXPECT_NEAR(std::abs(vertex.position.z), 1.0f, kTolerance);
        EXPECT_NEAR(Length(vertex.normal), 1.0f, kTolerance);
        // The vertex lies on the face its normal points out of: the plane at distance 1 along the normal
        EXPECT_NEAR(Dot(vertex.position, vertex.normal), 1.0f, kTolerance);
        faceNormals.insert({vertex.normal.x, vertex.normal.y, vertex.normal.z});
    }
    EXPECT_EQ(faceNormals.size(), 6u); // one per face
}

TEST(MakeCube, TangentsAreUnitPerpendicularToTheNormalAndPointAlongU) {
    const MeshData cube = MakeCube();

    for (const Vertex &vertex : cube.vertices) {
        EXPECT_NEAR(Length(vertex.tangent), 1.0f, kTolerance);
        EXPECT_NEAR(Dot(vertex.tangent, vertex.normal), 0.0f, kTolerance);
    }
    // The tangent points where u grows (normal mapping): a face is 2 units wide for u 0..1, so every edge's
    // projection on any corner's tangent is 2 * du.
    for (size_t i = 0; i < cube.indices.size(); i += 3) {
        for (size_t corner = 0; corner < 3; ++corner) {
            const XMFLOAT3 &tangent = cube.vertices[cube.indices[i + corner]].tangent;
            for (size_t edge = 0; edge < 3; ++edge) {
                const Vertex &a = cube.vertices[cube.indices[i + edge]];
                const Vertex &b = cube.vertices[cube.indices[i + (edge + 1) % 3]];
                EXPECT_NEAR(Dot(Sub(b.position, a.position), tangent), 2.0f * (b.uv.x - a.uv.x), kTolerance)
                    << "triangle " << i / 3 << ", corner " << corner << ", edge " << edge;
            }
        }
    }
}

TEST(MakeCube, TrianglesAreClockwiseSeenFromOutside) {
    const MeshData cube = MakeCube();

    for (size_t i = 0; i < cube.indices.size(); i += 3) {
        const Vertex &v0 = cube.vertices[cube.indices[i]];
        const Vertex &v1 = cube.vertices[cube.indices[i + 1]];
        const Vertex &v2 = cube.vertices[cube.indices[i + 2]];
        // Clockwise from outside (D3D's front face) <=> cross(v1 - v0, v2 - v0) along the normal
        const XMFLOAT3 faceNormal = Cross(Sub(v1.position, v0.position), Sub(v2.position, v0.position));
        EXPECT_GT(Dot(faceNormal, v0.normal), 0.0f) << "triangle " << i / 3;
        // All three corners belong to the same face
        EXPECT_NEAR(Dot(v1.normal, v0.normal), 1.0f, kTolerance);
        EXPECT_NEAR(Dot(v2.normal, v0.normal), 1.0f, kTolerance);
    }
}

TEST(MakeCube, EveryFaceShowsTheWholeTextureUpright) {
    const MeshData cube = MakeCube();

    for (size_t face = 0; face < 6; ++face) {
        std::set<std::tuple<float, float>> uvs;
        for (size_t corner = 0; corner < 4; ++corner) {
            const Vertex &vertex = cube.vertices[face * 4 + corner];
            EXPECT_GE(vertex.uv.x, 0.0f);
            EXPECT_LE(vertex.uv.x, 1.0f);
            EXPECT_GE(vertex.uv.y, 0.0f);
            EXPECT_LE(vertex.uv.y, 1.0f);
            uvs.insert({vertex.uv.x, vertex.uv.y});
            // On the side faces the top of the texture (v = 0) is at the top of the cube (y = +1)
            if (std::abs(vertex.normal.y) < kTolerance) {
                EXPECT_NEAR(vertex.uv.y, (1.0f - vertex.position.y) / 2.0f, kTolerance);
            }
        }
        EXPECT_EQ(uvs.size(), 4u) << "face " << face << " does not use all four corners of the texture";
    }
}
