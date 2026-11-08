#ifndef D3D12_ENGINE_SCENE_H
#define D3D12_ENGINE_SCENE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/Model.h"
#include "renderdata/RenderList.h"

namespace engine::scene {
    // Object -> world, row-vector convention (as in the RenderList)
    struct Transform {
        DirectX::XMFLOAT4X4 world{1.0f, 0.0f, 0.0f, 0.0f, //
                                  0.0f, 1.0f, 0.0f, 0.0f, //
                                  0.0f, 0.0f, 1.0f, 0.0f, //
                                  0.0f, 0.0f, 0.0f, 1.0f};
    };

    // The mesh and material an entity is drawn with
    struct MeshRenderer {
        render::MeshHandle mesh;
        render::MaterialHandle material;
    };

    // How an entity relates to its object: for recomputing its world when the object moves, and for picking
    struct EntityPart {
        DirectX::XMFLOAT4X4 toObject{}; // the part's transform, measured from the object's pivot
        render::Aabb bounds;            // around the mesh, in the mesh's own space
    };

    // Static objects never move and can live in GPU memory (Renderer::CreateStaticList); they can still be selected
    enum class Mobility : uint8_t { Static, Movable };

    // An index into Scene::Objects()
    struct ObjectId {
        uint32_t index = 0;

        bool operator==(const ObjectId &) const = default;
    };

    // What the user selects and moves: one or more parts that move together
    struct SceneObject {
        std::string name;
        Mobility mobility = Mobility::Movable;
        DirectX::XMFLOAT4X4 world{}; // places the pivot: the translation row is where the object is
        // Its parts are the entities firstEntity .. firstEntity + entityCount - 1
        uint32_t firstEntity = 0;
        uint32_t entityCount = 0;
    };

    // Objects, and their parts as entities in parallel arrays: entity i is Transforms()[i] + Renderers()[i] +
    // Parts()[i]. Nothing is removed yet, so an ObjectId stays valid as long as the scene.
    class Scene {
    public:
        // Adds an object with one entity per part, in part order. `pivot` is the point of the parts' common space
        // that `world` places: an entity's world is part.transform * translation(-pivot) * world.
        ObjectId AddObject(std::string name, std::span<const render::ModelPart> parts, const DirectX::XMFLOAT4X4 &world,
                           Mobility mobility, DirectX::XMFLOAT3 pivot = {});

        // Moves a Movable object and all its parts
        void SetObjectWorld(ObjectId id, const DirectX::XMFLOAT4X4 &world);

        // Draws every part of a Movable object with `material` (a static object's draws are already in GPU memory)
        void SetObjectMaterial(ObjectId id, render::MaterialHandle material);

        [[nodiscard]] const SceneObject &Object(ObjectId id) const;
        [[nodiscard]] std::span<const SceneObject> Objects() const noexcept { return m_objects; }

        [[nodiscard]] size_t EntityCount() const noexcept { return m_transforms.size(); }
        [[nodiscard]] std::span<const Transform> Transforms() const noexcept { return m_transforms; }
        [[nodiscard]] std::span<const MeshRenderer> Renderers() const noexcept { return m_renderers; }
        [[nodiscard]] std::span<const EntityPart> Parts() const noexcept { return m_parts; }

    private:
        std::vector<SceneObject> m_objects;
        std::vector<Transform> m_transforms;
        std::vector<MeshRenderer> m_renderers;
        std::vector<EntityPart> m_parts;
    };

    // Adds the entities of every object with this `mobility` to `list`, batched by mesh and material. No culling yet.
    void Extract(const Scene &scene, render::RenderList &list, Mobility mobility);
} // namespace engine::scene

#endif // D3D12_ENGINE_SCENE_H
