#include "scene/Scene.h"

#include <utility>

#include "core/Assert.h"

namespace engine::scene {
    ObjectId Scene::AddObject(std::string name, std::span<const render::ModelPart> parts,
                              const DirectX::XMFLOAT4X4 &world, Mobility mobility, DirectX::XMFLOAT3 pivot) {
        using namespace DirectX;

        const auto id = ObjectId{.index = static_cast<uint32_t>(m_objects.size())};
        const auto firstEntity = static_cast<uint32_t>(m_transforms.size());
        const XMMATRIX fromPivot = XMMatrixTranslation(-pivot.x, -pivot.y, -pivot.z);
        const XMMATRIX objectWorld = XMLoadFloat4x4(&world);
        for (const render::ModelPart &part : parts) {
            // Row vectors: a point goes through the part's transform first, then the pivot, then the object's world
            const XMMATRIX toObject = XMLoadFloat4x4(&part.transform) * fromPivot;
            EntityPart entityPart{.bounds = part.bounds};
            XMStoreFloat4x4(&entityPart.toObject, toObject);
            Transform transform;
            XMStoreFloat4x4(&transform.world, toObject * objectWorld);

            m_transforms.push_back(transform);
            m_renderers.push_back(MeshRenderer{.mesh = part.mesh, .material = part.material});
            m_parts.push_back(entityPart);
        }
        m_objects.push_back(SceneObject{.name = std::move(name),
                                        .mobility = mobility,
                                        .world = world,
                                        .firstEntity = firstEntity,
                                        .entityCount = static_cast<uint32_t>(parts.size())});
        return id;
    }

    void Scene::SetObjectWorld(ObjectId id, const DirectX::XMFLOAT4X4 &world) {
        using namespace DirectX;

        ENGINE_ASSERT_MSG(id.index < m_objects.size(), "Object {} out of range ({} objects)", id.index,
                          m_objects.size());
        SceneObject &object = m_objects[id.index];
        ENGINE_ASSERT_MSG(object.mobility == Mobility::Movable, "Object {} is static", object.name);

        object.world = world;
        const XMMATRIX objectWorld = XMLoadFloat4x4(&world);
        for (uint32_t i = object.firstEntity; i < object.firstEntity + object.entityCount; ++i) {
            XMStoreFloat4x4(&m_transforms[i].world, XMLoadFloat4x4(&m_parts[i].toObject) * objectWorld);
        }
    }

    void Scene::SetObjectMaterial(ObjectId id, render::MaterialHandle material) {
        ENGINE_ASSERT_MSG(id.index < m_objects.size(), "Object {} out of range ({} objects)", id.index,
                          m_objects.size());
        const SceneObject &object = m_objects[id.index];
        ENGINE_ASSERT_MSG(object.mobility == Mobility::Movable, "Object {} is static", object.name);

        for (uint32_t i = object.firstEntity; i < object.firstEntity + object.entityCount; ++i) {
            m_renderers[i].material = material;
        }
    }

    const SceneObject &Scene::Object(ObjectId id) const {
        ENGINE_ASSERT_MSG(id.index < m_objects.size(), "Object {} out of range ({} objects)", id.index,
                          m_objects.size());
        return m_objects[id.index];
    }

    void Extract(const Scene &scene, render::RenderList &list, Mobility mobility) {
        const std::span<const Transform> transforms = scene.Transforms();
        const std::span<const MeshRenderer> renderers = scene.Renderers();
        for (const SceneObject &object : scene.Objects()) {
            if (object.mobility != mobility) {
                continue;
            }
            for (uint32_t i = object.firstEntity; i < object.firstEntity + object.entityCount; ++i) {
                list.Add(render::BatchKey{.mesh = renderers[i].mesh, .material = renderers[i].material},
                         transforms[i].world);
            }
        }
    }
} // namespace engine::scene
