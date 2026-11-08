#include <numbers>
#include <vector>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/Model.h"
#include "renderdata/RenderList.h"
#include "scene/Scene.h"

using engine::render::Aabb;
using engine::render::BatchKey;
using engine::render::MaterialHandle;
using engine::render::MeshHandle;
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
        return scene.AddObject("cube", {&part, 1}, Store(DirectX::XMMatrixTranslation(x, 0, 0)), mobility);
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
    const ObjectId id = scene.AddObject("model", parts, world, Mobility::Movable);

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
    const ObjectId id = scene.AddObject("model", {&part, 1}, world, Mobility::Movable, {1, 2, 3});

    ExpectTranslation(scene.Object(id).world, 10, 0, 0);
    ExpectTranslation(scene.Transforms()[0].world, 8, -4, -6);
}

TEST(Scene, SetObjectWorldMovesEveryPartForTheNextExtract) {
    const std::vector<ModelPart> parts{ModelPart{.transform = Store(DirectX::XMMatrixTranslation(1, 0, 0))},
                                       ModelPart{}};
    Scene scene;
    const ObjectId id = scene.AddObject("model", parts, Store(DirectX::XMMatrixIdentity()), Mobility::Movable);

    scene.SetObjectWorld(id, Store(DirectX::XMMatrixTranslation(0, 7, 0)));
    RenderList list;
    Extract(scene, list, Mobility::Movable);

    ASSERT_EQ(list.Buckets().size(), 1u);
    ASSERT_EQ(list.Buckets()[0].worlds.size(), 2u);
    ExpectTranslation(list.Buckets()[0].worlds[0], 1, 7, 0);
    ExpectTranslation(list.Buckets()[0].worlds[1], 0, 7, 0);
    ExpectTranslation(scene.Object(id).world, 0, 7, 0);
}

// Every part of that object changes, and no other object's
TEST(Scene, SetObjectMaterialChangesOnlyThatObject) {
    const std::vector<ModelPart> parts{ModelPart{.material = MaterialHandle{1}},
                                       ModelPart{.material = MaterialHandle{2}}};
    Scene scene;
    const ObjectId id = scene.AddObject("model", parts, Store(DirectX::XMMatrixIdentity()), Mobility::Movable);
    AddCube(scene, ModelPart{.material = MaterialHandle{3}}, 0);

    scene.SetObjectMaterial(id, MaterialHandle{9});

    EXPECT_EQ(scene.Renderers()[0].material, MaterialHandle{9});
    EXPECT_EQ(scene.Renderers()[1].material, MaterialHandle{9});
    EXPECT_EQ(scene.Renderers()[2].material, MaterialHandle{3});
}
