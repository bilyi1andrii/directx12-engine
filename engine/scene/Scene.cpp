#include "scene/Scene.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>

#include "core/Assert.h"

namespace engine::scene {
    ObjectId Scene::AddObject(std::string name, const render::Model &model, const DirectX::XMFLOAT4X4 &world,
                              Mobility mobility, DirectX::XMFLOAT3 pivot) {
        using namespace DirectX;

        const auto id = ObjectId{.index = static_cast<uint32_t>(m_objects.size())};
        const auto firstEntity = static_cast<uint32_t>(m_transforms.size());
        const XMMATRIX fromPivot = XMMatrixTranslation(-pivot.x, -pivot.y, -pivot.z);
        const XMMATRIX objectWorld = XMLoadFloat4x4(&world);
        for (const render::ModelPart &part : model.parts) {
            // Row vectors: first the part's transform, then the pivot, then the object's world
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
                                        .entityCount = static_cast<uint32_t>(model.parts.size())});
        return id;
    }

    ObjectId Scene::AddLight(std::string name, const render::Model &marker, const DirectX::XMFLOAT4X4 &world,
                             const render::LocalLight &light) {
        const ObjectId id = AddObject(std::move(name), marker, world, Mobility::Movable);
        m_lightObjects.push_back(id);
        m_lights.push_back(light);
        return id;
    }

    render::LocalLight *Scene::Light(ObjectId id) {
        const auto found = std::ranges::find(m_lightObjects, id);
        return found != m_lightObjects.end() ? &m_lights[static_cast<size_t>(found - m_lightObjects.begin())] : nullptr;
    }

    const render::LocalLight *Scene::Light(ObjectId id) const {
        const auto found = std::ranges::find(m_lightObjects, id);
        return found != m_lightObjects.end() ? &m_lights[static_cast<size_t>(found - m_lightObjects.begin())] : nullptr;
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

    void Scene::SetObjectPosition(ObjectId id, const DirectX::XMFLOAT3 &position) {
        // Row vectors: the translation is the 4th row; the rest holds the rotation and scale
        DirectX::XMFLOAT4X4 world = Object(id).world;
        world._41 = position.x;
        world._42 = position.y;
        world._43 = position.z;
        SetObjectWorld(id, world);
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

    DirectX::XMFLOAT3 Scene::ObjectPosition(ObjectId id) const {
        const DirectX::XMFLOAT4X4 &world = Object(id).world;
        return {world._41, world._42, world._43};
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

    void Extract(const Scene &scene, render::LightSet &lights) {
        using namespace DirectX;

        lights.environment = scene.Environment();
        lights.points.clear();
        lights.spots.clear();
        const std::span<const ObjectId> lightObjects = scene.LightObjects();
        const std::span<const render::LocalLight> sceneLights = scene.Lights();
        for (size_t i = 0; i < sceneLights.size(); ++i) {
            const render::LocalLight &light = sceneLights[i];
            // Row vectors: the 4th row is the translation, the 3rd is where the object's +Z axis points
            const XMFLOAT4X4 &world = scene.Object(lightObjects[i]).world;
            const XMFLOAT3 position{world._41, world._42, world._43};
            if (light.type == render::LightType::Point) {
                lights.points.push_back({.light = light, .position = position});
            } else {
                // The row's length is the object's Z scale. Scaling the marker must not change the light.
                XMFLOAT3 direction{};
                XMStoreFloat3(&direction, XMVector3Normalize(XMVectorSet(world._31, world._32, world._33, 0.0f)));
                lights.spots.push_back({.light = light, .position = position, .direction = direction});
            }
        }
    }
} // namespace engine::scene
