#ifndef D3D12_ENGINE_STRESSGRID_H
#define D3D12_ENGINE_STRESSGRID_H

// The bench's scene: a square grid of copies of one model, its lights and the camera path over it. A static grid is
// uploaded once, a dynamic one every frame.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <DirectXMath.h>

#include "common/GridSetup.h"
#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
#include "renderdata/Model.h"
#include "renderdata/RenderList.h"
#include "scene/CameraPath.h"

namespace render {
    class GpuResources;
} // namespace render

namespace bench {
    inline constexpr uint32_t kStressPathFrames = 1000;

    class StressGrid {
    public:
        StressGrid(render::GpuResources &resources, apps::GridMode mode, uint32_t count, const render::Model &model);

        void Submit(render::RenderList &list) const;

        [[nodiscard]] std::span<const render::StaticListHandle> StaticLists() const noexcept;

        [[nodiscard]] size_t ObjectCount() const noexcept { return size_t{m_side} * m_side; }
        [[nodiscard]] float Extent() const noexcept;

        [[nodiscard]] std::array<scene::CameraKeyframe, 5> CameraPath() const noexcept;

        [[nodiscard]] render::LightSet Lights() const;

    private:
        apps::GridMode m_mode;
        uint32_t m_side;
        std::vector<render::ModelPart> m_parts;
        DirectX::XMFLOAT4X4 m_base;
        std::optional<render::StaticListHandle> m_staticList;
    };
} // namespace bench

#endif // D3D12_ENGINE_STRESSGRID_H
