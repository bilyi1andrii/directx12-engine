#include <array>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "scene/Camera.h"

TEST(Camera, LookAtFacesTheTargetFromEveryDirection) {
    using namespace DirectX;
    const XMFLOAT3 target{0.0f, -4.0f, 0.0f};
    // On each horizontal axis (the DX11 formula looked the wrong way from z == 0), above and below, and off-axis
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
