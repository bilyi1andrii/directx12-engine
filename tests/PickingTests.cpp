// Tests picking: the ray through a pixel, hitting boxes placed by a world matrix, and the nearest object.

#include <cmath>
#include <optional>
#include <vector>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "assets/Primitives.h"
#include "renderdata/Model.h"
#include "scene/Camera.h"
#include "scene/Picking.h"
#include "scene/Scene.h"

namespace {
    using namespace DirectX;
    using assets::kCubeBounds;
    using render::Aabb;
    using render::Model;
    using render::ModelPart;
    using scene::Camera;
    using scene::IntersectHorizontalPlane;
    using scene::IntersectRayBox;
    using scene::MakePickRay;
    using scene::Mobility;
    using scene::ObjectId;
    using scene::Pick;
    using scene::Ray;
    using scene::Scene;

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

    [[nodiscard]] std::optional<float> IntersectCube(const Ray &ray, const XMFLOAT4X4 &world) {
        return IntersectRayBox(ray, world, kCubeBounds);
    }

    ObjectId AddCube(Scene &scene, FXMMATRIX world) {
        const ModelPart part{.bounds = kCubeBounds};
        return scene.AddObject("cube", Model{.parts = {part}}, ToFloat4x4(world), Mobility::Movable);
    }
} // namespace

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

    const std::optional<float> hit = IntersectCube(ray, ToFloat4x4(XMMatrixIdentity()));
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 9.0f - camera.nearZ, 1e-3f);
}

TEST(Picking, OffCentrePixelHitsTheBoxDrawnThere) {
    const Camera camera;
    const Ray right = MakePickRay(camera, {600.0f, 400.0f}, {800.0f, 800.0f});
    EXPECT_TRUE(IntersectCube(right, ToFloat4x4(XMMatrixTranslation(5.0f, 0.0f, 10.0f))).has_value());
    EXPECT_FALSE(IntersectCube(right, ToFloat4x4(XMMatrixTranslation(0.0f, 0.0f, 10.0f))).has_value());

    const Ray up = MakePickRay(camera, {400.0f, 200.0f}, {800.0f, 800.0f});
    EXPECT_TRUE(IntersectCube(up, ToFloat4x4(XMMatrixTranslation(0.0f, 5.0f, 10.0f))).has_value());
    EXPECT_FALSE(IntersectCube(up, ToFloat4x4(XMMatrixTranslation(0.0f, -5.0f, 10.0f))).has_value());
}

TEST(Picking, RotatedBoxIsHitWhereItSticksOut) {
    const Ray ray{.origin = {-10.0f, 0.0f, 1.2f}, .direction = {1.0f, 0.0f, 0.0f}};
    EXPECT_FALSE(IntersectCube(ray, ToFloat4x4(XMMatrixIdentity())).has_value());

    const std::optional<float> hit = IntersectCube(ray, ToFloat4x4(XMMatrixRotationY(XM_PIDIV4)));
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 10.0f - (std::sqrt(2.0f) - 1.2f), 1e-4f);
}

TEST(Picking, NearerBoxHasTheSmallerDistance) {
    const Ray ray{.origin = {0.0f, 0.0f, -10.0f}, .direction = {0.0f, 0.0f, 1.0f}};
    const std::optional<float> nearBox = IntersectCube(ray, ToFloat4x4(XMMatrixIdentity()));
    const std::optional<float> farBox =
        IntersectCube(ray, ToFloat4x4(XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.0f, 5.0f)));
    ASSERT_TRUE(nearBox.has_value());
    ASSERT_TRUE(farBox.has_value());
    EXPECT_NEAR(*nearBox, 9.0f, 1e-4f);
    EXPECT_NEAR(*farBox, 13.0f, 1e-4f);
    EXPECT_LT(*nearBox, *farBox);

    EXPECT_EQ(
        IntersectCube({.origin = {0.0f, 0.0f, 0.0f}, .direction = {0.0f, 0.0f, 1.0f}}, ToFloat4x4(XMMatrixIdentity())),
        0.0f);
    EXPECT_FALSE(IntersectCube(ray, ToFloat4x4(XMMatrixTranslation(0.0f, 0.0f, -20.0f))).has_value());
}

TEST(Picking, BoxCornersAreTheSlabs) {
    const Aabb box{.min = {2.0f, -1.0f, -1.0f}, .max = {4.0f, 1.0f, 1.0f}};
    const XMFLOAT4X4 world = ToFloat4x4(XMMatrixTranslation(0.0f, 0.0f, 5.0f));

    const std::optional<float> hit =
        IntersectRayBox({.origin = {3.0f, 0.0f, -10.0f}, .direction = {0.0f, 0.0f, 1.0f}}, world, box);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 14.0f, 1e-4f);
    EXPECT_FALSE(IntersectRayBox({.origin = {0.0f, 0.0f, -10.0f}, .direction = {0.0f, 0.0f, 1.0f}}, world, box));
}

TEST(Picking, FlatBoxIsHitWhereTheRayCrossesIt) {
    const Aabb floor{.min = {-1.0f, 0.0f, -1.0f}, .max = {1.0f, 0.0f, 1.0f}};
    const XMFLOAT4X4 world = ToFloat4x4(XMMatrixIdentity());

    const std::optional<float> hit =
        IntersectRayBox({.origin = {0.5f, 5.0f, 0.0f}, .direction = {0.0f, -1.0f, 0.0f}}, world, floor);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 5.0f, 1e-4f);
    EXPECT_FALSE(IntersectRayBox({.origin = {3.0f, 5.0f, 0.0f}, .direction = {0.0f, -1.0f, 0.0f}}, world, floor));
    EXPECT_FALSE(IntersectRayBox({.origin = {-5.0f, 1.0f, 0.0f}, .direction = {1.0f, 0.0f, 0.0f}}, world, floor));
}

TEST(Pick, SelectsTheObjectOfTheNearestPart) {
    Scene scene;
    AddCube(scene, XMMatrixTranslation(0.0f, 0.0f, 10.0f));
    const std::vector<ModelPart> parts{
        ModelPart{.transform = ToFloat4x4(XMMatrixTranslation(30.0f, 0.0f, 0.0f)), .bounds = kCubeBounds},
        ModelPart{.bounds = kCubeBounds}};
    const ObjectId model = scene.AddObject("model", Model{.parts = parts},
                                           ToFloat4x4(XMMatrixTranslation(0.0f, 0.0f, 2.0f)), Mobility::Static);

    EXPECT_EQ(Pick(scene, {.origin = {0.0f, 0.0f, -10.0f}, .direction = {0.0f, 0.0f, 1.0f}}), model);
    EXPECT_FALSE(Pick(scene, {.origin = {9.0f, 0.0f, -10.0f}, .direction = {0.0f, 0.0f, 1.0f}}).has_value());
}

TEST(Pick, BoxesTheRayStartsInsideDoNotCount) {
    Scene scene;
    AddCube(scene, XMMatrixScaling(20.0f, 20.0f, 20.0f));
    const Ray ray{.origin = {0.0f, 0.0f, -5.0f}, .direction = {0.0f, 0.0f, 1.0f}};
    EXPECT_FALSE(Pick(scene, ray).has_value());

    const ObjectId cube = AddCube(scene, XMMatrixTranslation(0.0f, 0.0f, 5.0f));
    EXPECT_EQ(Pick(scene, ray), cube);
}

TEST(MakePickRay, EdgesOfTheViewFollowTheFieldOfViewAndScreenDirections) {
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
    scene::LookAt(camera, {2.0f, -4.0f, 4.0f});

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

    EXPECT_FALSE(IntersectHorizontalPlane(down, 6.0f).has_value());
    const Ray level{.origin = {0.0f, 5.0f, 0.0f}, .direction = {0.0f, 0.0f, 1.0f}};
    EXPECT_FALSE(IntersectHorizontalPlane(level, 0.0f).has_value());
}
