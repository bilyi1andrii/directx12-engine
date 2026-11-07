#include <cmath>
#include <optional>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "scene/Camera.h"
#include "scene/Picking.h"

using DirectX::XMFLOAT3;
using engine::scene::Camera;
using engine::scene::IntersectHorizontalPlane;
using engine::scene::Ray;
using engine::scene::ScreenRay;

namespace {
    constexpr float kTolerance = 1e-4f;

    void ExpectNear(const XMFLOAT3 &actual, const XMFLOAT3 &expected) {
        EXPECT_NEAR(actual.x, expected.x, kTolerance);
        EXPECT_NEAR(actual.y, expected.y, kTolerance);
        EXPECT_NEAR(actual.z, expected.z, kTolerance);
    }

    [[nodiscard]] XMFLOAT3 Normalized(float x, float y, float z) {
        const float length = std::sqrt(x * x + y * y + z * z);
        return {x / length, y / length, z / length};
    }
} // namespace

TEST(ScreenRay, CentreOfTheViewLooksAlongTheCamera) {
    Camera camera;
    camera.position = {1.0f, 2.0f, 3.0f};
    camera.rotation = {0.3f, 0.5f, 0.0f}; // looking a little down and to the right

    const Ray ray = ScreenRay(camera, 640.0f, 360.0f, 1280.0f, 720.0f);

    ExpectNear(ray.origin, camera.position);
    XMFLOAT3 forward{};
    DirectX::XMStoreFloat3(&forward, DirectX::XMVector3Normalize(engine::scene::Forward(camera)));
    ExpectNear(ray.direction, forward);
}

TEST(ScreenRay, EdgesOfTheViewFollowTheFieldOfViewAndScreenDirections) {
    // Looking along +Z with a 90 degree vertical field of view: the top edge is 45 degrees up. A view twice as wide
    // as high reaches twice as far sideways. Screen y grows downwards, world y upwards.
    Camera camera;
    camera.fovDegrees = 90.0f;
    constexpr float kWidth = 200.0f;
    constexpr float kHeight = 100.0f;

    ExpectNear(ScreenRay(camera, 100.0f, 0.0f, kWidth, kHeight).direction, Normalized(0.0f, 1.0f, 1.0f));
    ExpectNear(ScreenRay(camera, 100.0f, kHeight, kWidth, kHeight).direction, Normalized(0.0f, -1.0f, 1.0f));
    ExpectNear(ScreenRay(camera, kWidth, 50.0f, kWidth, kHeight).direction, Normalized(2.0f, 0.0f, 1.0f));
    ExpectNear(ScreenRay(camera, 0.0f, 50.0f, kWidth, kHeight).direction, Normalized(-2.0f, 0.0f, 1.0f));
}

TEST(ScreenRay, CentreOfTheViewHitsThePointTheCameraLooksAt) {
    Camera camera;
    camera.position = {5.0f, 10.0f, -3.0f};
    engine::scene::LookAt(camera, {2.0f, -4.0f, 4.0f});

    const std::optional<XMFLOAT3> hit =
        IntersectHorizontalPlane(ScreenRay(camera, 400.0f, 300.0f, 800.0f, 600.0f), -4.0f);

    ASSERT_TRUE(hit.has_value());
    ExpectNear(*hit, {2.0f, -4.0f, 4.0f});
}

TEST(IntersectHorizontalPlane, HitsInFrontOnly) {
    const Ray down{.origin = {1.0f, 5.0f, 2.0f}, .direction = Normalized(1.0f, -1.0f, 0.0f)};
    const std::optional<XMFLOAT3> hit = IntersectHorizontalPlane(down, 1.0f);
    ASSERT_TRUE(hit.has_value());
    ExpectNear(*hit, {5.0f, 1.0f, 2.0f});

    // The plane is above a ray that points down: behind it
    EXPECT_FALSE(IntersectHorizontalPlane(down, 6.0f).has_value());
    // A level ray never meets a horizontal plane
    const Ray level{.origin = {0.0f, 5.0f, 0.0f}, .direction = {0.0f, 0.0f, 1.0f}};
    EXPECT_FALSE(IntersectHorizontalPlane(level, 0.0f).has_value());
}
