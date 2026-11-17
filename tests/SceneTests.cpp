#include <numbers>
#include <vector>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/Lights.h"
#include "renderdata/Model.h"
#include "renderdata/RenderList.h"
#include "scene/Scene.h"

using engine::render::Aabb;
using engine::render::BatchKey;
using engine::render::LightSet;
using engine::render::LightType;
using engine::render::LocalLight;
using engine::render::MaterialHandle;
using engine::render::MeshHandle;
using engine::render::Model;
using engine::render::ModelPart;
using engine::render::RenderList;
using engine::scene::Extract;
using engine::scene::Mobility;
using engine::scene::ObjectId;
using engine::scene::Scene;

namespace {
    [[nodiscard]] DirectX::XMFLOAT4X4 Store(DirectX::FXMMATRIX matrix) {
        DirectX::XMFLOAT4X4 result{};
        DirectX::XMStoreFloat4x4(&result, matrix);
        return result;
    }

    // Row-vector convention: the translation is the 4th row
    void ExpectTranslation(const DirectX::XMFLOAT4X4 &matrix, float x, float y, float z) {
        constexpr float kTolerance = 1e-5f;
        EXPECT_NEAR(matrix._41, x, kTolerance);
        EXPECT_NEAR(matrix._42, y, kTolerance);
        EXPECT_NEAR(matrix._43, z, kTolerance);
    }

    // An object with one part, at (x, 0, 0)
    ObjectId AddCube(Scene &scene, ModelPart part, float x, Mobility mobility = Mobility::Movable) {
        return scene.AddObject("cube", Model{.parts = {part}}, Store(DirectX::XMMatrixTranslation(x, 0, 0)), mobility);
    }
} // namespace

TEST(Scene, ExtractBatchesEntitiesByMeshAndMaterial) {
    Scene scene;
    const ModelPart brick{.mesh = MeshHandle{1}, .material = MaterialHandle{2}};
    const ModelPart glass{.mesh = MeshHandle{1}, .material = MaterialHandle{3}};
    AddCube(scene, brick, 1);
    AddCube(scene, glass, 2);
    AddCube(scene, brick, 3);

    RenderList list;
    Extract(scene, list, Mobility::Movable);

    ASSERT_EQ(list.Buckets().size(), 2u);
    EXPECT_EQ(list.Buckets()[0].key, (BatchKey{.mesh = MeshHandle{1}, .material = MaterialHandle{2}}));
    ASSERT_EQ(list.Buckets()[0].worlds.size(), 2u);
    ExpectTranslation(list.Buckets()[0].worlds[1], 3, 0, 0);
    EXPECT_EQ(list.Buckets()[1].key, (BatchKey{.mesh = MeshHandle{1}, .material = MaterialHandle{3}}));
    EXPECT_EQ(list.InstanceCount(), 3u);
}

// Static objects go into a static list once, movable ones into the frame's list
TEST(Scene, ExtractTakesOnlyTheRequestedMobility) {
    Scene scene;
    AddCube(scene, ModelPart{}, 1, Mobility::Static);
    AddCube(scene, ModelPart{}, 2, Mobility::Movable);

    RenderList movable;
    Extract(scene, movable, Mobility::Movable);
    RenderList fixed;
    Extract(scene, fixed, Mobility::Static);

    ASSERT_EQ(movable.InstanceCount(), 1u);
    ExpectTranslation(movable.Buckets()[0].worlds[0], 2, 0, 0);
    ASSERT_EQ(fixed.InstanceCount(), 1u);
    ExpectTranslation(fixed.Buckets()[0].worlds[0], 1, 0, 0);
}

// A part is placed inside the object first, then the object in the world. The object turns 90 degrees around Y
// (+X becomes -Z) and moves to (10, 0, 0), so the part's offset (1, 0, 0) ends up at (10, 0, -1).
// The other order would give (11, 0, 0).
TEST(Scene, AddObjectPlacesPartsInsideTheObject) {
    const std::vector<ModelPart> parts{ModelPart{.mesh = MeshHandle{4},
                                                 .material = MaterialHandle{5},
                                                 .transform = Store(DirectX::XMMatrixTranslation(1, 0, 0)),
                                                 .bounds = Aabb{.min = {-2, -2, -2}, .max = {2, 2, 2}}},
                                       ModelPart{.mesh = MeshHandle{6}, .material = MaterialHandle{7}}}; // identity
    const DirectX::XMFLOAT4X4 world =
        Store(DirectX::XMMatrixRotationY(std::numbers::pi_v<float> / 2) * DirectX::XMMatrixTranslation(10, 0, 0));

    Scene scene;
    const ObjectId id = scene.AddObject("model", Model{.parts = parts}, world, Mobility::Movable);

    EXPECT_EQ(scene.Object(id).name, "model");
    EXPECT_EQ(scene.Object(id).entityCount, 2u);
    ASSERT_EQ(scene.EntityCount(), 2u);
    ExpectTranslation(scene.Transforms()[0].world, 10, 0, -1);
    ExpectTranslation(scene.Transforms()[1].world, 10, 0, 0);
    EXPECT_EQ(scene.Renderers()[0].mesh, MeshHandle{4});
    EXPECT_EQ(scene.Renderers()[0].material, MaterialHandle{5});
    EXPECT_EQ(scene.Renderers()[1].mesh, MeshHandle{6});
    EXPECT_FLOAT_EQ(scene.Parts()[0].bounds.max.x, 2.0f);
}

// The pivot (1, 2, 3) lands exactly at the object's translation (10, 0, 0). The part's origin is (-1, -2, -3) away
// from the pivot, doubled by the scale: (10 - 2, -4, -6).
TEST(Scene, WorldPlacesThePivot) {
    const ModelPart part{};
    const DirectX::XMFLOAT4X4 world = Store(DirectX::XMMatrixScaling(2, 2, 2) * DirectX::XMMatrixTranslation(10, 0, 0));

    Scene scene;
    const ObjectId id = scene.AddObject("model", Model{.parts = {part}}, world, Mobility::Movable, {1, 2, 3});

    ExpectTranslation(scene.Object(id).world, 10, 0, 0);
    ExpectTranslation(scene.Transforms()[0].world, 8, -4, -6);
}

TEST(Scene, SetObjectWorldMovesEveryPartForTheNextExtract) {
    const std::vector<ModelPart> parts{ModelPart{.transform = Store(DirectX::XMMatrixTranslation(1, 0, 0))},
                                       ModelPart{}};
    Scene scene;
    const ObjectId id =
        scene.AddObject("model", Model{.parts = parts}, Store(DirectX::XMMatrixIdentity()), Mobility::Movable);

    scene.SetObjectWorld(id, Store(DirectX::XMMatrixTranslation(0, 7, 0)));
    RenderList list;
    Extract(scene, list, Mobility::Movable);

    ASSERT_EQ(list.Buckets().size(), 1u);
    ASSERT_EQ(list.Buckets()[0].worlds.size(), 2u);
    ExpectTranslation(list.Buckets()[0].worlds[0], 1, 7, 0);
    ExpectTranslation(list.Buckets()[0].worlds[1], 0, 7, 0);
    ExpectTranslation(scene.Object(id).world, 0, 7, 0);
}

// The object is scaled by 2 and turned 90 degrees around Y (+X becomes -Z). A new position moves the pivot there and
// keeps both, so the part's offset (1, 0, 0) still ends up (0, 0, -2) away from it.
TEST(Scene, SetObjectPositionKeepsRotationAndScale) {
    const DirectX::XMFLOAT4X4 world =
        Store(DirectX::XMMatrixScaling(2, 2, 2) * DirectX::XMMatrixRotationY(std::numbers::pi_v<float> / 2) *
              DirectX::XMMatrixTranslation(10, 0, 0));
    const ModelPart part{.transform = Store(DirectX::XMMatrixTranslation(1, 0, 0))};
    Scene scene;
    const ObjectId id = scene.AddObject("model", Model{.parts = {part}}, world, Mobility::Movable);

    scene.SetObjectPosition(id, {3, 4, 5});

    const DirectX::XMFLOAT3 position = scene.ObjectPosition(id);
    EXPECT_FLOAT_EQ(position.x, 3.0f);
    EXPECT_FLOAT_EQ(position.y, 4.0f);
    EXPECT_FLOAT_EQ(position.z, 5.0f);
    ExpectTranslation(scene.Transforms()[0].world, 3, 4, 3);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_EQ(scene.Object(id).world.m[row][column], world.m[row][column]) << row << ", " << column;
        }
    }
}

// Every part of that object changes, and no other object's
TEST(Scene, SetObjectMaterialChangesOnlyThatObject) {
    const std::vector<ModelPart> parts{ModelPart{.material = MaterialHandle{1}},
                                       ModelPart{.material = MaterialHandle{2}}};
    Scene scene;
    const ObjectId id =
        scene.AddObject("model", Model{.parts = parts}, Store(DirectX::XMMatrixIdentity()), Mobility::Movable);
    AddCube(scene, ModelPart{.material = MaterialHandle{3}}, 0);

    scene.SetObjectMaterial(id, MaterialHandle{9});

    EXPECT_EQ(scene.Renderers()[0].material, MaterialHandle{9});
    EXPECT_EQ(scene.Renderers()[1].material, MaterialHandle{9});
    EXPECT_EQ(scene.Renderers()[2].material, MaterialHandle{3});
}

// A light stands at its object's translation. The object is scaled unevenly and then turned 90 degrees around Y
// (+Z becomes +X), so a spot light points along +X, at unit length whatever the scale.
TEST(Scene, ExtractPlacesLightsWhereTheirObjectsStand) {
    Scene scene;
    scene.AddLight("point", Model{}, Store(DirectX::XMMatrixTranslation(1, 2, 3)),
                   LocalLight{.type = LightType::Point, .strength = 3.5f});
    scene.AddLight("spot", Model{},
                   Store(DirectX::XMMatrixScaling(2, 3, 5) * DirectX::XMMatrixRotationY(std::numbers::pi_v<float> / 2) *
                         DirectX::XMMatrixTranslation(4, 5, 6)),
                   LocalLight{.type = LightType::Spot, .strength = 1.5f});

    LightSet lights;
    Extract(scene, lights);

    constexpr float kTolerance = 1e-5f;
    ASSERT_EQ(lights.points.size(), 1u);
    EXPECT_EQ(lights.points[0].light.strength, 3.5f);
    EXPECT_EQ(lights.points[0].position.x, 1.0f);
    EXPECT_EQ(lights.points[0].position.y, 2.0f);
    EXPECT_EQ(lights.points[0].position.z, 3.0f);
    ASSERT_EQ(lights.spots.size(), 1u);
    EXPECT_EQ(lights.spots[0].light.strength, 1.5f);
    EXPECT_EQ(lights.spots[0].position.x, 4.0f);
    EXPECT_EQ(lights.spots[0].position.z, 6.0f);
    EXPECT_NEAR(lights.spots[0].direction.x, 1.0f, kTolerance);
    EXPECT_NEAR(lights.spots[0].direction.y, 0.0f, kTolerance);
    EXPECT_NEAR(lights.spots[0].direction.z, 0.0f, kTolerance);
}

// Moving the object moves its light: there is no second copy of the position to keep in step
TEST(Scene, ExtractFollowsAMovedLightAndReplacesTheOldList) {
    Scene scene;
    scene.Environment().ambientStrength = 0.75f;
    const ObjectId id = scene.AddLight("point", Model{}, Store(DirectX::XMMatrixIdentity()), LocalLight{});
    LightSet lights;
    Extract(scene, lights);

    scene.SetObjectPosition(id, {7, 8, 9});
    Extract(scene, lights);

    EXPECT_EQ(lights.environment.ambientStrength, 0.75f);
    ASSERT_EQ(lights.points.size(), 1u); // not added a second time
    EXPECT_EQ(lights.points[0].position.x, 7.0f);
    EXPECT_EQ(lights.points[0].position.y, 8.0f);
    EXPECT_EQ(lights.points[0].position.z, 9.0f);
}

// A disabled light is still extracted, so that what the renderer gets is the scene as it is
TEST(Scene, OnlyLightObjectsHaveALight) {
    Scene scene;
    const ObjectId cube = AddCube(scene, ModelPart{}, 1);
    const ObjectId lamp = scene.AddLight("lamp", Model{.parts = {ModelPart{}}}, Store(DirectX::XMMatrixIdentity()),
                                         LocalLight{.type = LightType::Spot});

    EXPECT_EQ(scene.Light(cube), nullptr);
    ASSERT_NE(scene.Light(lamp), nullptr);
    EXPECT_EQ(scene.Object(lamp).mobility, Mobility::Movable);
    EXPECT_EQ(scene.Object(lamp).entityCount, 1u); // its marker is drawn and picked like any object

    scene.Light(lamp)->enabled = false;
    LightSet lights;
    Extract(scene, lights);

    EXPECT_TRUE(lights.points.empty());
    ASSERT_EQ(lights.spots.size(), 1u);
    EXPECT_FALSE(lights.spots[0].light.enabled);
}
