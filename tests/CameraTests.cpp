#include <array>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "scene/Camera.h"

TEST(Camera, LookAtFacesTheTargetFromEveryDirection) {
    using namespace DirectX;
    const XMFLOAT3 target{0.0f, -4.0f, 0.0f};
    // On both axes (DX11's formula failed at z == 0), above, below and off-axis
    constexpr std::array kPositions{
        XMFLOAT3{10.0f, 5.0f, 0.0f},   XMFLOAT3{-10.0f, 5.0f, 0.0f}, XMFLOAT3{0.0f, 5.0f, 10.0f},
        XMFLOAT3{0.0f, -9.0f, -10.0f}, XMFLOAT3{3.0f, 4.0f, -8.0f},  XMFLOAT3{-7.0f, -1.0f, 6.0f},
    };
    for (const XMFLOAT3 &position : kPositions) {
        engine::scene::Camera camera;
        camera.position = position;
        engine::scene::LookAt(camera, target);

        XMFLOAT3 forward{};
        XMStoreFloat3(&forward, engine::scene::Forward(camera));
        XMFLOAT3 expected{};
        XMStoreFloat3(&expected, XMVector3Normalize(XMLoadFloat3(&target) - XMLoadFloat3(&position)));
        EXPECT_NEAR(forward.x, expected.x, 1e-5f) << "from " << position.x << ", " << position.y << ", " << position.z;
        EXPECT_NEAR(forward.y, expected.y, 1e-5f) << "from " << position.x << ", " << position.y << ", " << position.z;
        EXPECT_NEAR(forward.z, expected.z, 1e-5f) << "from " << position.x << ", " << position.y << ", " << position.z;
    }
}

// Reversed-Z: the near plane gets depth 1, the far plane 0, and a nearer point always the greater depth
TEST(Camera, ProjectionReversesDepth) {
    using namespace DirectX;
    const engine::scene::Camera camera;
    const XMMATRIX projection = engine::scene::ProjectionMatrix(camera, 16.0f / 9.0f);
    const auto depthAt = [&](float viewZ) {
        return XMVectorGetZ(XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, viewZ, 1.0f), projection));
    };
    EXPECT_NEAR(depthAt(camera.nearZ), 1.0f, 1e-6f);
    EXPECT_NEAR(depthAt(camera.farZ), 0.0f, 1e-6f);
    EXPECT_GT(depthAt(10.0f), depthAt(11.0f));
}
