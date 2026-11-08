#include <array>
#include <cstddef>
#include <numbers>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/RenderList.h"

using engine::render::Aabb;
using engine::render::BatchKey;
using engine::render::FitToCell;
using engine::render::MaterialHandle;
using engine::render::MeshHandle;
using engine::render::ModelPart;
using engine::render::RenderList;
using engine::render::SubmitGrid;

namespace {
    constexpr BatchKey kKeyA{.mesh = MeshHandle{1}, .material = MaterialHandle{0}};
    constexpr BatchKey kKeyB{.mesh = MeshHandle{1}, .material = MaterialHandle{2}};

    [[nodiscard]] DirectX::XMFLOAT4X4 Translation(float x, float y, float z) {
        DirectX::XMFLOAT4X4 matrix{};
        DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixTranslation(x, y, z));
        return matrix;
    }

    // Row-vector convention: the translation is the 4th row
    void ExpectTranslation(const DirectX::XMFLOAT4X4 &matrix, float x, float y, float z) {
        constexpr float kTolerance = 1e-5f;
        EXPECT_NEAR(matrix._41, x, kTolerance);
        EXPECT_NEAR(matrix._42, y, kTolerance);
        EXPECT_NEAR(matrix._43, z, kTolerance);
    }

    void ExpectTransformedPoint(const DirectX::XMFLOAT4X4 &matrix, const DirectX::XMFLOAT3 &point, float x, float y,
                                float z) {
        constexpr float kTolerance = 1e-5f;
        DirectX::XMFLOAT3 result{};
        DirectX::XMStoreFloat3(
            &result, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), DirectX::XMLoadFloat4x4(&matrix)));
        EXPECT_NEAR(result.x, x, kTolerance);
        EXPECT_NEAR(result.y, y, kTolerance);
        EXPECT_NEAR(result.z, z, kTolerance);
    }
} // namespace

TEST(RenderList, GroupsObjectsByKeyInFirstAddedOrder) {
    RenderList list;
    list.Add(kKeyA, Translation(1, 0, 0));
    list.Add(kKeyB, Translation(2, 0, 0));
    list.Add(kKeyA, Translation(3, 0, 0));

    ASSERT_EQ(list.Buckets().size(), 2u);
    EXPECT_EQ(list.InstanceCount(), 3u);
    EXPECT_EQ(list.Buckets()[0].key, kKeyA);
    EXPECT_EQ(list.Buckets()[1].key, kKeyB);
    // Objects keep their order within a bucket
    ASSERT_EQ(list.Buckets()[0].worlds.size(), 2u);
    ExpectTranslation(list.Buckets()[0].worlds[0], 1, 0, 0);
    ExpectTranslation(list.Buckets()[0].worlds[1], 3, 0, 0);
    ASSERT_EQ(list.Buckets()[1].worlds.size(), 1u);
    ExpectTranslation(list.Buckets()[1].worlds[0], 2, 0, 0);
}

TEST(RenderList, ClearRemovesEveryObjectAndBucketButKeepsTheirMemory) {
    RenderList list;
    for (int i = 0; i < 100; ++i) {
        list.Add(kKeyA, Translation(1, 0, 0));
    }
    list.Add(kKeyB, Translation(2, 0, 0));

    list.Clear();

    EXPECT_TRUE(list.Buckets().empty());
    EXPECT_EQ(list.InstanceCount(), 0u);

    // The next frame's first bucket gets the first bucket's memory back, so refilling it does not reallocate
    list.Add(kKeyA, Translation(1, 0, 0));
    ASSERT_EQ(list.Buckets().size(), 1u);
    EXPECT_EQ(list.Buckets()[0].worlds.size(), 1u);
    EXPECT_GE(list.Buckets()[0].worlds.capacity(), 100u);
}

TEST(SubmitGrid, CentresTheGridOnTheBaseWithOffsetsInWorldSpace) {
    // A base turned 90 degrees around Y: the grid offsets must not turn with it (DX11 submitGrid)
    DirectX::XMFLOAT4X4 base{};
    DirectX::XMStoreFloat4x4(&base, DirectX::XMMatrixRotationY(std::numbers::pi_v<float> / 2) *
                                        DirectX::XMMatrixTranslation(10, -4, 0));
    const ModelPart part{.mesh = kKeyA.mesh, .material = kKeyA.material};
    RenderList list;

    SubmitGrid(list, {&part, 1}, base, 3, 3, 3.0f, 2.0f);

    ASSERT_EQ(list.Buckets().size(), 1u);
    const auto &worlds = list.Buckets()[0].worlds;
    ASSERT_EQ(worlds.size(), 9u);
    // Added row by row: [0] is (row 0, col 0), [4] the centre, [8] (row 2, col 2)
    ExpectTranslation(worlds[0], 10 - 3, -4, 0 - 2);
    ExpectTranslation(worlds[4], 10, -4, 0);
    ExpectTranslation(worlds[8], 10 + 3, -4, 0 + 2);
    // Every copy keeps the base's rotation: the first row is (cos 90, 0, -sin 90)
    EXPECT_NEAR(worlds[0]._11, 0.0f, 1e-5f);
    EXPECT_NEAR(worlds[0]._13, -1.0f, 1e-5f);
}

TEST(SubmitGrid, GivesEachPartABucketAndAppliesItsTransformBeforeTheBase) {
    const std::array parts{
        ModelPart{.mesh = kKeyA.mesh, .material = kKeyA.material, .transform = Translation(1, 0, 0)},
        ModelPart{.mesh = kKeyB.mesh, .material = kKeyB.material, .transform = Translation(0, 0, 5)},
    };
    // The base doubles the size: a part's own offset doubles too, the grid's offsets do not
    DirectX::XMFLOAT4X4 base{};
    DirectX::XMStoreFloat4x4(&base, DirectX::XMMatrixScaling(2, 2, 2) * DirectX::XMMatrixTranslation(10, -4, 0));
    RenderList list;

    SubmitGrid(list, parts, base, 1, 2, 3.0f, 3.0f);

    ASSERT_EQ(list.Buckets().size(), 2u);
    EXPECT_EQ(list.InstanceCount(), 4u);
    EXPECT_EQ(list.Buckets()[0].key, kKeyA);
    EXPECT_EQ(list.Buckets()[1].key, kKeyB);
    ASSERT_EQ(list.Buckets()[0].worlds.size(), 2u);
    ASSERT_EQ(list.Buckets()[1].worlds.size(), 2u);
    ExpectTranslation(list.Buckets()[0].worlds[0], 10 + 2 - 1.5f, -4, 0);
    ExpectTranslation(list.Buckets()[0].worlds[1], 10 + 2 + 1.5f, -4, 0);
    ExpectTranslation(list.Buckets()[1].worlds[0], 10 - 1.5f, -4, 10);
    ExpectTranslation(list.Buckets()[1].worlds[1], 10 + 1.5f, -4, 10);
}

TEST(FitToCell, OnlyMovesACubeThatAlreadyFits) {
    const Aabb cube{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    const DirectX::XMFLOAT4X4 fitted = FitToCell(cube, 2.0f, {0.0f, -4.0f, 0.0f});

    // Exact, not near: the cube grid must keep the same matrices
    const DirectX::XMFLOAT4X4 expected = Translation(0, -4, 0);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            EXPECT_EQ(fitted.m[row][col], expected.m[row][col]);
        }
    }
}

TEST(FitToCell, ScalesByTheLargestSideAndCentresTheBox) {
    // 4 x 2 x 4, not centred on its origin
    const Aabb bounds{.min = {10.0f, 0.0f, -2.0f}, .max = {14.0f, 2.0f, 2.0f}};

    const DirectX::XMFLOAT4X4 fitted = FitToCell(bounds, 2.0f, {0.0f, -4.0f, 0.0f});

    ExpectTransformedPoint(fitted, bounds.min, -1.0f, -4.5f, -1.0f);
    ExpectTransformedPoint(fitted, bounds.max, 1.0f, -3.5f, 1.0f);
}

TEST(FitToCell, KeepsTheScaleOfAModelWithNoSize) {
    const Aabb point{.min = {3.0f, 3.0f, 3.0f}, .max = {3.0f, 3.0f, 3.0f}};

    const DirectX::XMFLOAT4X4 fitted = FitToCell(point, 2.0f, {1.0f, 2.0f, 3.0f});

    EXPECT_EQ(fitted._11, 1.0f);
    ExpectTransformedPoint(fitted, point.min, 1.0f, 2.0f, 3.0f);
}
