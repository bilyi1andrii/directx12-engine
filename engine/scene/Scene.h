#ifndef D3D12_ENGINE_SCENE_H
#define D3D12_ENGINE_SCENE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
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

    // What an entity needs to follow its object and to be picked
    struct EntityPart {
        DirectX::XMFLOAT4X4 toObject{}; // the part's place relative to the object's pivot
        render::Aabb bounds;            // box around the mesh, in mesh space
    };

    // Static objects never move, so they are uploaded once; they can still be selected
    enum class Mobility : uint8_t { Static, Movable };

    // An index into Scene::Objects()
    using ObjectId = render::Handle<struct ObjectTag>;

    // What the user selects and moves: one or more parts that move together
    struct SceneObject {
        std::string name;
        Mobility mobility = Mobility::Movable;
        DirectX::XMFLOAT4X4 world{}; // its translation row is where the object stands
        // Its parts: entityCount entities, starting at firstEntity
        uint32_t firstEntity = 0;
        uint32_t entityCount = 0;
    };

    // Objects and their parts (entities). Entity i is Transforms()[i] + Renderers()[i] + Parts()[i]. Nothing is removed
    // yet, so an ObjectId stays valid as long as the scene.
    // A point or spot light is an object too: it stands and points where its object does. Light that has no place
    // (ambient, the directional light) is the scene's Environment().
    class Scene {
    public:
        // Adds `model` as an object with one entity per part. `world` places `pivot`, a point in model space, so an
        // entity's world is part.transform * translation(-pivot) * world.
        ObjectId AddObject(std::string name, const render::Model &model, const DirectX::XMFLOAT4X4 &world,
                           Mobility mobility, DirectX::XMFLOAT3 pivot = {});

        // Adds a Movable object that is also a point or spot light. It is drawn with `marker`, which may be an empty
        // Model: then there is nothing to draw or click. The light shines from `world`'s translation, and a spot
        // light points along `world`'s +Z axis.
        ObjectId AddLight(std::string name, const render::Model &marker, const DirectX::XMFLOAT4X4 &world,
                          const render::LocalLight &light);

        // The light of an object, to read or edit; nullptr if the object is not a light. Valid until the next
        // AddLight. Searches every light: a scene has a dozen.
        [[nodiscard]] render::LocalLight *Light(ObjectId id);
        [[nodiscard]] const render::LocalLight *Light(ObjectId id) const;

        [[nodiscard]] render::LightEnvironment &Environment() noexcept { return m_environment; }
        [[nodiscard]] const render::LightEnvironment &Environment() const noexcept { return m_environment; }

        // Moves a Movable object and all its parts
        void SetObjectWorld(ObjectId id, const DirectX::XMFLOAT4X4 &world);

        // Moves a Movable object so that its pivot stands at `position`, keeping its rotation and scale
        void SetObjectPosition(ObjectId id, const DirectX::XMFLOAT3 &position);

        // Gives every part of a Movable object this material (a static object's draws are already on the GPU)
        void SetObjectMaterial(ObjectId id, render::MaterialHandle material);

        [[nodiscard]] const SceneObject &Object(ObjectId id) const;
        // Where the object's pivot stands in the world
        [[nodiscard]] DirectX::XMFLOAT3 ObjectPosition(ObjectId id) const;
        [[nodiscard]] std::span<const SceneObject> Objects() const noexcept { return m_objects; }

        [[nodiscard]] size_t EntityCount() const noexcept { return m_transforms.size(); }
        [[nodiscard]] std::span<const Transform> Transforms() const noexcept { return m_transforms; }
        [[nodiscard]] std::span<const MeshRenderer> Renderers() const noexcept { return m_renderers; }
        [[nodiscard]] std::span<const EntityPart> Parts() const noexcept { return m_parts; }

        // Light i is Lights()[i], on object LightObjects()[i]
        [[nodiscard]] std::span<const ObjectId> LightObjects() const noexcept { return m_lightObjects; }
        [[nodiscard]] std::span<const render::LocalLight> Lights() const noexcept { return m_lights; }

    private:
        std::vector<SceneObject> m_objects;
        std::vector<Transform> m_transforms;
        std::vector<MeshRenderer> m_renderers;
        std::vector<EntityPart> m_parts;
        std::vector<ObjectId> m_lightObjects;
        std::vector<render::LocalLight> m_lights;
        render::LightEnvironment m_environment;
    };

    // Adds every object with this `mobility` to `list`, batched by mesh and material. No culling yet.
    void Extract(const Scene &scene, render::RenderList &list, Mobility mobility);

    // Replaces `lights` with the scene's environment and all its lights, disabled ones too, in world space.
    // A light's position is its object's translation; a spot's direction is the object's +Z axis at unit length,
    // whatever the object's scale.
    void Extract(const Scene &scene, render::LightSet &lights);
} // namespace engine::scene

#endif // D3D12_ENGINE_SCENE_H
