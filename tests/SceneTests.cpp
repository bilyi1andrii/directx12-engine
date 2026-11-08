#include <numbers>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/Model.h"
#include "renderdata/RenderList.h"
#include "scene/Scene.h"

using engine::render::BatchKey;
using engine::render::MaterialHandle;
using engine::render::MeshHandle;
using engine::render::Model;
using engine::render::ModelPart;
using engine::render::RenderList;
using engine::scene::Entity;
using engine::scene::Extract;
using engine::scene::MeshRenderer;
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
} // namespace

TEST(Scene, ExtractBatchesEntitiesByMeshAndMaterial) {
    Scene scene;
    const MeshRenderer brick{.mesh = MeshHandle{1}, .material = MaterialHandle{2}};
    const MeshRenderer glass{.mesh = MeshHandle{1}, .material = MaterialHandle{3}};
    scene.Add(brick, Store(DirectX::XMMatrixTranslation(1, 0, 0)));
    scene.Add(glass, Store(DirectX::XMMatrixTranslation(2, 0, 0)));
    scene.Add(brick, Store(DirectX::XMMatrixTranslation(3, 0, 0)));

    RenderList list;
    Extract(scene, list);

    ASSERT_EQ(list.Buckets().size(), 2u);
    EXPECT_EQ(list.Buckets()[0].key, (BatchKey{.mesh = MeshHandle{1}, .material = MaterialHandle{2}}));
    ASSERT_EQ(list.Buckets()[0].worlds.size(), 2u);
    ExpectTranslation(list.Buckets()[0].worlds[1], 3, 0, 0);
    EXPECT_EQ(list.Buckets()[1].key, (BatchKey{.mesh = MeshHandle{1}, .material = MaterialHandle{3}}));
    EXPECT_EQ(list.InstanceCount(), 3u);
}

// A part is placed inside the model first, then the model in the world. The model turns 90 degrees around Y
// (+X becomes -Z) and moves to (10, 0, 0), so the part's offset (1, 0, 0) ends up at (10, 0, -1).
// The other order would give (11, 0, 0).
TEST(Scene, InstantiatePlacesPartsInsideTheModel) {
    Model model;
    model.parts.push_back(ModelPart{.mesh = MeshHandle{4},
                                    .material = MaterialHandle{5},
                                    .transform = Store(DirectX::XMMatrixTranslation(1, 0, 0))});
    model.parts.push_back(ModelPart{.mesh = MeshHandle{6}, .material = MaterialHandle{7}}); // identity
    const DirectX::XMFLOAT4X4 world =
        Store(DirectX::XMMatrixRotationY(std::numbers::pi_v<float> / 2) * DirectX::XMMatrixTranslation(10, 0, 0));

    Scene scene;
    const std::vector<Entity> entities = scene.Instantiate(model, world);

    ASSERT_EQ(entities.size(), 2u);
    EXPECT_EQ(scene.EntityCount(), 2u);
    ExpectTranslation(scene.World(entities[0]), 10, 0, -1);
    ExpectTranslation(scene.World(entities[1]), 10, 0, 0);
    EXPECT_EQ(scene.Renderers()[entities[0].index].mesh, MeshHandle{4});
    EXPECT_EQ(scene.Renderers()[entities[0].index].material, MaterialHandle{5});
    EXPECT_EQ(scene.Renderers()[entities[1].index].mesh, MeshHandle{6});
}

TEST(Scene, SetWorldMovesAnEntityForTheNextExtract) {
    Scene scene;
    const Entity entity = scene.Add(MeshRenderer{}, Store(DirectX::XMMatrixIdentity()));

    scene.SetWorld(entity, Store(DirectX::XMMatrixTranslation(0, 7, 0)));
    RenderList list;
    Extract(scene, list);

    ASSERT_EQ(list.Buckets().size(), 1u);
    ExpectTranslation(list.Buckets()[0].worlds[0], 0, 7, 0);
}
