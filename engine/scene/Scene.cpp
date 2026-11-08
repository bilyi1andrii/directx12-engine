#include "scene/Scene.h"

#include "core/Assert.h"

namespace engine::scene {
    Entity Scene::Add(const MeshRenderer &renderer, const DirectX::XMFLOAT4X4 &world) {
        m_transforms.push_back(Transform{.world = world});
        m_renderers.push_back(renderer);
        return Entity{.index = static_cast<uint32_t>(m_transforms.size() - 1)};
    }

    std::vector<Entity> Scene::Instantiate(const render::Model &model, const DirectX::XMFLOAT4X4 &world) {
        using namespace DirectX;

        const XMMATRIX modelWorld = XMLoadFloat4x4(&world);
        std::vector<Entity> entities;
        entities.reserve(model.parts.size());
        XMFLOAT4X4 partWorld{};
        for (const render::ModelPart &part : model.parts) {
            // Row vectors: a point goes through the part's transform first, then the model's
            XMStoreFloat4x4(&partWorld, XMLoadFloat4x4(&part.transform) * modelWorld);
            entities.push_back(Add(MeshRenderer{.mesh = part.mesh, .material = part.material}, partWorld));
        }
        return entities;
    }

    void Scene::SetWorld(Entity entity, const DirectX::XMFLOAT4X4 &world) {
        ENGINE_ASSERT_MSG(entity.index < m_transforms.size(), "Entity {} out of range ({} entities)", entity.index,
                          m_transforms.size());
        m_transforms[entity.index].world = world;
    }

    const DirectX::XMFLOAT4X4 &Scene::World(Entity entity) const {
        ENGINE_ASSERT_MSG(entity.index < m_transforms.size(), "Entity {} out of range ({} entities)", entity.index,
                          m_transforms.size());
        return m_transforms[entity.index].world;
    }

    void Extract(const Scene &scene, render::RenderList &list) {
        const std::span<const Transform> transforms = scene.Transforms();
        const std::span<const MeshRenderer> renderers = scene.Renderers();
        for (size_t i = 0; i < transforms.size(); ++i) {
            list.Add(render::BatchKey{.mesh = renderers[i].mesh, .material = renderers[i].material},
                     transforms[i].world);
        }
    }
} // namespace engine::scene
