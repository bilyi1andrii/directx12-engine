#ifndef D3D12_ENGINE_SCENE_H
#define D3D12_ENGINE_SCENE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/Model.h"
#include "renderdata/RenderList.h"

namespace engine::scene {
    // An index into the scene's component arrays
    struct Entity {
        uint32_t index = 0;

        bool operator==(const Entity &) const = default;
    };

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

    // Entities as components in parallel arrays: entity i is Transforms()[i] + Renderers()[i]. Nothing is
    // removed yet, so an Entity stays valid as long as the scene.
    class Scene {
    public:
        // Adds one entity drawn with `renderer` at `world`
        Entity Add(const MeshRenderer &renderer, const DirectX::XMFLOAT4X4 &world);

        // One entity per part of `model`, at part.transform * world. Returns them in part order.
        std::vector<Entity> Instantiate(const render::Model &model, const DirectX::XMFLOAT4X4 &world);

        void SetWorld(Entity entity, const DirectX::XMFLOAT4X4 &world);
        [[nodiscard]] const DirectX::XMFLOAT4X4 &World(Entity entity) const;

        [[nodiscard]] size_t EntityCount() const noexcept { return m_transforms.size(); }
        [[nodiscard]] std::span<const Transform> Transforms() const noexcept { return m_transforms; }
        [[nodiscard]] std::span<const MeshRenderer> Renderers() const noexcept { return m_renderers; }

    private:
        std::vector<Transform> m_transforms;
        std::vector<MeshRenderer> m_renderers;
    };

    // Adds every entity of `scene` to `list`, batched by mesh and material. No culling yet.
    void Extract(const Scene &scene, render::RenderList &list);
} // namespace engine::scene

#endif // D3D12_ENGINE_SCENE_H
