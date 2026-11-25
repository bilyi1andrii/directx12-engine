#include <array>
#include <cmath>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/ShadowView.h"

using engine::render::Aabb;
using engine::render::MakeShadowView;
using engine::render::ShadowView;

namespace {
    constexpr float kTolerance = 1e-4f;

    [[nodiscard]] DirectX::XMFLOAT3 Project(const ShadowView &view, const DirectX::XMFLOAT3 &point) {
        using namespace DirectX;
        DirectX::XMFLOAT3 clip{};
        XMStoreFloat3(&clip, XMVector3TransformCoord(XMLoadFloat3(&point), XMLoadFloat4x4(&view.viewProjection)));
        return clip;
    }

    constexpr Aabb kCube{.min = {-10.0f, -10.0f, -10.0f}, .max = {10.0f, 10.0f, 10.0f}};
} // namespace

TEST(ShadowView, CenterIsInTheMiddleAtHalfDepth) {
    const ShadowView view = MakeShadowView({0.0f, -1.0f, 0.0f}, kCube);
    const DirectX::XMFLOAT3 clip = Project(view, {0.0f, 0.0f, 0.0f});
    EXPECT_NEAR(clip.x, 0.0f, kTolerance);
    EXPECT_NEAR(clip.y, 0.0f, kTolerance);
    EXPECT_NEAR(clip.z, 0.5f, kTolerance);
}

TEST(ShadowView, NearerToTheLightIsDeeper) {
    const ShadowView view = MakeShadowView({0.0f, -1.0f, 0.0f}, kCube);
    EXPECT_GT(Project(view, {0.0f, 5.0f, 0.0f}).z, Project(view, {0.0f, -5.0f, 0.0f}).z);
}

TEST(ShadowView, EveryCornerIsInView) {
    for (const DirectX::XMFLOAT3 direction :
         {DirectX::XMFLOAT3{1.0f, -1.0f, 1.0f}, DirectX::XMFLOAT3{0.0f, -1.0f, 0.0f},
          DirectX::XMFLOAT3{-0.5f, -1.0f, 1.0f}}) {
        const ShadowView view = MakeShadowView(direction, kCube);
        for (int corner = 0; corner < 8; ++corner) {
            const DirectX::XMFLOAT3 point{(corner & 1) ? 10.0f : -10.0f, (corner & 2) ? 10.0f : -10.0f,
                                          (corner & 4) ? 10.0f : -10.0f};
            const DirectX::XMFLOAT3 clip = Project(view, point);
            EXPECT_LE(std::abs(clip.x), 1.0f + kTolerance);
            EXPECT_LE(std::abs(clip.y), 1.0f + kTolerance);
            EXPECT_GE(clip.z, -kTolerance);
            EXPECT_LE(clip.z, 1.0f + kTolerance);
        }
    }
}

TEST(ShadowView, TransformBoxHoldsTheTurnedBox) {
    using namespace DirectX;
    XMFLOAT4X4 world{};
    XMStoreFloat4x4(&world, XMMatrixRotationY(XM_PIDIV4) * XMMatrixTranslation(5.0f, 0.0f, 0.0f));
    const Aabb box = engine::render::TransformBox({.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}}, world);
    const float half = std::sqrt(2.0f);
    EXPECT_NEAR(box.min.x, 5.0f - half, kTolerance);
    EXPECT_NEAR(box.max.x, 5.0f + half, kTolerance);
    EXPECT_NEAR(box.min.y, -1.0f, kTolerance);
    EXPECT_NEAR(box.max.z, half, kTolerance);
}
