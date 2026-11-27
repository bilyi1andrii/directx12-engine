#ifndef D3D12_ENGINE_SCENE_H
#define D3D12_ENGINE_SCENE_H

// The objects of the world: each object has parts that move together, and a point or spot light is an object too.
// The scene hands the renderer its draws and its lights.

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

namespace scene {
    struct Transform {
        DirectX::XMFLOAT4X4 world{1.0f, 0.0f, 0.0f, 0.0f, //
                                  0.0f, 1.0f, 0.0f, 0.0f, //
                                  0.0f, 0.0f, 1.0f, 0.0f, //
                                  0.0f, 0.0f, 0.0f, 1.0f};
    };

    struct MeshRenderer {
        render::MeshHandle mesh;
        render::MaterialHandle material;
    };

    // one model part, indexed like transforms and renderers
    struct EntityPart {
        DirectX::XMFLOAT4X4 toObject{}; // part to object, pivot applied
        render::Aabb bounds;
    };

    // static objects upload once, can be selected but not moved
    enum class Mobility : uint8_t { Static, Movable };

    using ObjectId = render::Handle<struct ObjectTag>;

    struct SceneObject {
        std::string name;
        Mobility mobility = Mobility::Movable;
        DirectX::XMFLOAT4X4 world{};
        uint32_t firstEntity = 0; // range in transforms, renderers and parts
        uint32_t entityCount = 0;
    };

    class Scene {
    public:
        // pivot is point of model placed at object origin
        ObjectId AddObject(std::string name, const render::Model &model, const DirectX::XMFLOAT4X4 &world,
                           Mobility mobility, DirectX::XMFLOAT3 pivot = {});

        ObjectId AddLight(std::string name, const render::Model &marker, const DirectX::XMFLOAT4X4 &world,
                          const render::LocalLight &light);

        [[nodiscard]] render::LocalLight *Light(ObjectId id);
        [[nodiscard]] const render::LocalLight *Light(ObjectId id) const;

        [[nodiscard]] render::LightEnvironment &Environment() noexcept { return m_environment; }
        [[nodiscard]] const render::LightEnvironment &Environment() const noexcept { return m_environment; }

        void SetObjectWorld(ObjectId id, const DirectX::XMFLOAT4X4 &world);

        void SetObjectPosition(ObjectId id, const DirectX::XMFLOAT3 &position);

        void SetObjectMaterial(ObjectId id, render::MaterialHandle material);

        [[nodiscard]] const SceneObject &Object(ObjectId id) const;
        [[nodiscard]] DirectX::XMFLOAT3 ObjectPosition(ObjectId id) const;
        [[nodiscard]] std::span<const SceneObject> Objects() const noexcept { return m_objects; }

        [[nodiscard]] std::span<const Transform> Transforms() const noexcept { return m_transforms; }
        [[nodiscard]] std::span<const MeshRenderer> Renderers() const noexcept { return m_renderers; }
        [[nodiscard]] std::span<const EntityPart> Parts() const noexcept { return m_parts; }

        [[nodiscard]] std::span<const ObjectId> LightObjects() const noexcept { return m_lightObjects; }
        [[nodiscard]] std::span<const render::LocalLight> Lights() const noexcept { return m_lights; }

    private:
        std::vector<SceneObject> m_objects;
        // one entry per part, same index in all three
        std::vector<Transform> m_transforms;
        std::vector<MeshRenderer> m_renderers;
        std::vector<EntityPart> m_parts;
        // light components, side by side with m_lights
        std::vector<ObjectId> m_lightObjects;
        std::vector<render::LocalLight> m_lights;
        render::LightEnvironment m_environment;
    };

    void Extract(const Scene &scene, render::RenderList &list, Mobility mobility);

    void Extract(const Scene &scene, render::LightSet &lights);
} // namespace scene

#endif // D3D12_ENGINE_SCENE_H
