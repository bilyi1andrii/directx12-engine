#ifndef D3D12_ENGINE_STRESSGRID_H
#define D3D12_ENGINE_STRESSGRID_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <DirectXMath.h>

#include "common/GridSetup.h"
#include "render/GpuResources.h"
#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
#include "renderdata/Model.h"
#include "renderdata/RenderList.h"
#include "scene/CameraPath.h"

namespace engine::bench {
    // Length of the grid's camera path in frames, the same as the bench's default --frames
    inline constexpr uint32_t kStressPathFrames = 1000;

    // A square grid of copies of a model, 4 units below the origin. Every copy is scaled to the size of a grid cube.
    class StressGrid {
    public:
        // `count` is copies of the whole model, rounded up to a square. Uploads a static grid.
        StressGrid(render::GpuResources &resources, apps::GridMode mode, uint32_t count, const render::Model &model);

        // Call every frame after list.Clear(): adds the grid in dynamic mode, does nothing in static mode
        void Submit(render::RenderList &list) const;

        // Pass to Renderer::Render every frame; empty in dynamic mode
        [[nodiscard]] std::span<const render::StaticListHandle> StaticLists() const noexcept;

        [[nodiscard]] size_t ObjectCount() const noexcept { return size_t{m_side} * m_side; }
        // Width of the grid in world units, edge to edge
        [[nodiscard]] float Extent() const noexcept;

        // The bench's camera path: one loop around the grid in kStressPathFrames frames, always looking at its
        // centre. Distance and height grow with the grid. The DX11 engine copies this formula.
        [[nodiscard]] std::array<scene::CameraKeyframe, 5> CameraPath() const noexcept;

        // The bench lights: ambient, specular and directional light,
        // plus two point and two spot lights
        [[nodiscard]] render::LightSet Lights() const;

    private:
        apps::GridMode m_mode;
        uint32_t m_side;
        std::vector<render::ModelPart> m_parts;
        DirectX::XMFLOAT4X4 m_base;
        std::optional<render::StaticListHandle> m_staticList; // static mode only
    };
} // namespace engine::bench

#endif // D3D12_ENGINE_STRESSGRID_H
