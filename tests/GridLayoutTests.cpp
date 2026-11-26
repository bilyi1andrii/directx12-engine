// Tests placing grid cells around the grid's center and fitting a model into its cell.

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/GridLayout.h"

using render::Aabb;
using render::FitToCell;
using render::GridCellOffset;

namespace {
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

TEST(GridCellOffset, PlacesTheCellsEvenlyAroundTheCenter) {
    const DirectX::XMFLOAT3 first = GridCellOffset(0, 0, 3, 2, 3.0f, 2.0f);
    EXPECT_FLOAT_EQ(first.x, -1.5f);
    EXPECT_FLOAT_EQ(first.y, 0.0f);
    EXPECT_FLOAT_EQ(first.z, -2.0f);

    const DirectX::XMFLOAT3 middleRow = GridCellOffset(1, 1, 3, 2, 3.0f, 2.0f);
    EXPECT_FLOAT_EQ(middleRow.x, 1.5f);
    EXPECT_FLOAT_EQ(middleRow.z, 0.0f);

    const DirectX::XMFLOAT3 last = GridCellOffset(2, 1, 3, 2, 3.0f, 2.0f);
    EXPECT_FLOAT_EQ(last.x, 1.5f);
    EXPECT_FLOAT_EQ(last.z, 2.0f);

    const DirectX::XMFLOAT3 only = GridCellOffset(0, 0, 1, 1, 3.0f, 3.0f);
    EXPECT_FLOAT_EQ(only.x, 0.0f);
    EXPECT_FLOAT_EQ(only.z, 0.0f);
}

TEST(FitToCell, ScalesByTheLargestSideAndCentresTheBox) {
    const Aabb bounds{.min = {10.0f, 0.0f, -2.0f}, .max = {14.0f, 2.0f, 2.0f}};

    const DirectX::XMFLOAT4X4 fitted = FitToCell(bounds, 2.0f, {0.0f, -4.0f, 0.0f});

    ExpectTransformedPoint(fitted, bounds.min, -1.0f, -4.5f, -1.0f);
    ExpectTransformedPoint(fitted, bounds.max, 1.0f, -3.5f, 1.0f);
}
