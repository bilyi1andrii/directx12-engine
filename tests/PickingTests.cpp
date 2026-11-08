#include <cmath>
#include <optional>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "scene/Camera.h"
#include "scene/Picking.h"

namespace {
    using namespace DirectX;
    using engine::scene::Camera;
    using engine::scene::IntersectHorizontalPlane;
    using engine::scene::IntersectRayBox;
    using engine::scene::MakePickRay;
    using engine::scene::Ray;

    constexpr float kTolerance = 1e-4f;

    XMFLOAT4X4 ToFloat4x4(FXMMATRIX matrix) {
        XMFLOAT4X4 result{};
        XMStoreFloat4x4(&result, matrix);
        return result;
    }

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

// Camera at z = -10 looking along +Z: the ray through the viewport's centre starts on the near plane, goes straight
// ahead and enters the cube at the origin through its -Z face
TEST(Picking, CentreRayHitsTheBoxInFront) {
    Camera camera;
    camera.position = {0.0f, 0.0f, -10.0f};
    const Ray ray = MakePickRay(camera, {640.0f, 360.0f}, {1280.0f, 720.0f});
    EXPECT_NEAR(ray.origin.x, 0.0f, 1e-4f);
    EXPECT_NEAR(ray.origin.y, 0.0f, 1e-4f);
    EXPECT_NEAR(ray.origin.z, -10.0f + camera.nearZ, 1e-4f);
    EXPECT_NEAR(ray.direction.x, 0.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.y, 0.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.z, 1.0f, 1e-5f);

    const std::optional<float> hit = IntersectRayBox(ray, ToFloat4x4(XMMatrixIdentity()));
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 9.0f - camera.nearZ, 1e-3f);
}

// 90 degrees vertical field of view in a square viewport: half the viewport right of the centre is 45 degrees off,
// so at depth 10 the ray is 5 units to the right; half the viewport up is 5 units up (pixel y grows downwards)
TEST(Picking, OffCentrePixelHitsTheBoxDrawnThere) {
    const Camera camera; // at the origin, looking along +Z, 90 degrees
    const Ray right = MakePickRay(camera, {600.0f, 400.0f}, {800.0f, 800.0f});
    EXPECT_TRUE(IntersectRayBox(right, ToFloat4x4(XMMatrixTranslation(5.0f, 0.0f, 10.0f))).has_value());
    EXPECT_FALSE(IntersectRayBox(right, ToFloat4x4(XMMatrixTranslation(0.0f, 0.0f, 10.0f))).has_value());

    const Ray up = MakePickRay(camera, {400.0f, 200.0f}, {800.0f, 800.0f});
    EXPECT_TRUE(IntersectRayBox(up, ToFloat4x4(XMMatrixTranslation(0.0f, 5.0f, 10.0f))).has_value());
    EXPECT_FALSE(IntersectRayBox(up, ToFloat4x4(XMMatrixTranslation(0.0f, -5.0f, 10.0f))).has_value());
}

// Turned 45 degrees about Y, the cube's vertical edges stick out to sqrt(2) along X and Z. A ray along +X at z = 1.2
// passes beside the unturned cube but hits the turned one at x = -(sqrt(2) - 1.2)
TEST(Picking, RotatedBoxIsHitWhereItSticksOut) {
    const Ray ray{.origin = {-10.0f, 0.0f, 1.2f}, .direction = {1.0f, 0.0f, 0.0f}};
    EXPECT_FALSE(IntersectRayBox(ray, ToFloat4x4(XMMatrixIdentity())).has_value());

    const std::optional<float> hit = IntersectRayBox(ray, ToFloat4x4(XMMatrixRotationY(XM_PIDIV4)));
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 10.0f - (std::sqrt(2.0f) - 1.2f), 1e-4f);
}

// The sandbox selects the smallest distance, so distances must be world units along the ray, also for a scaled box
TEST(Picking, NearerBoxHasTheSmallerDistance) {
    const Ray ray{.origin = {0.0f, 0.0f, -10.0f}, .direction = {0.0f, 0.0f, 1.0f}};
    const std::optional<float> nearBox = IntersectRayBox(ray, ToFloat4x4(XMMatrixIdentity())); // z in [-1, 1]
    const std::optional<float> farBox =
        IntersectRayBox(ray, ToFloat4x4(XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.0f, 5.0f)));
    ASSERT_TRUE(nearBox.has_value());
    ASSERT_TRUE(farBox.has_value());
    EXPECT_NEAR(*nearBox, 9.0f, 1e-4f);
    EXPECT_NEAR(*farBox, 13.0f, 1e-4f); // z in [3, 7]
    EXPECT_LT(*nearBox, *farBox);

    // From inside a box the hit is at once; a box behind the ray's origin is not hit
    EXPECT_EQ(IntersectRayBox({.origin = {0.0f, 0.0f, 0.0f}, .direction = {0.0f, 0.0f, 1.0f}},
                              ToFloat4x4(XMMatrixIdentity())),
              0.0f);
    EXPECT_FALSE(IntersectRayBox(ray, ToFloat4x4(XMMatrixTranslation(0.0f, 0.0f, -20.0f))).has_value());
}

TEST(MakePickRay, CentreOfTheViewLooksAlongTheCamera) {
    Camera camera;
    camera.position = {1.0f, 2.0f, 3.0f};
    camera.rotation = {0.3f, 0.5f, 0.0f}; // looking a little down and to the right

    const Ray ray = MakePickRay(camera, {640.0f, 360.0f}, {1280.0f, 720.0f});

    XMFLOAT3 forward{};
    XMStoreFloat3(&forward, XMVector3Normalize(engine::scene::Forward(camera)));
    ExpectNear(ray.direction, forward);
}

TEST(MakePickRay, EdgesOfTheViewFollowTheFieldOfViewAndScreenDirections) {
    // Along +Z with a 90 degree vertical FOV: the top edge is 45 degrees up, a 2:1 view reaches twice as far
    // sideways. Screen y grows down, world y up.
    Camera camera;
    camera.fovDegrees = 90.0f;
    const XMFLOAT2 viewport{200.0f, 100.0f};

    ExpectNear(MakePickRay(camera, {100.0f, 0.0f}, viewport).direction, Normalized(0.0f, 1.0f, 1.0f));
    ExpectNear(MakePickRay(camera, {100.0f, 100.0f}, viewport).direction, Normalized(0.0f, -1.0f, 1.0f));
    ExpectNear(MakePickRay(camera, {200.0f, 50.0f}, viewport).direction, Normalized(2.0f, 0.0f, 1.0f));
    ExpectNear(MakePickRay(camera, {0.0f, 50.0f}, viewport).direction, Normalized(-2.0f, 0.0f, 1.0f));
}

TEST(MakePickRay, CentreOfTheViewHitsThePointTheCameraLooksAt) {
    Camera camera;
    camera.position = {5.0f, 10.0f, -3.0f};
    engine::scene::LookAt(camera, {2.0f, -4.0f, 4.0f});

    const std::optional<XMFLOAT3> hit =
        IntersectHorizontalPlane(MakePickRay(camera, {400.0f, 300.0f}, {800.0f, 600.0f}), -4.0f);

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
