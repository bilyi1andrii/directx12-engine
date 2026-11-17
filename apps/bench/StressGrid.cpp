#include "StressGrid.h"

#include "core/Log.h"
#include "render/GpuResources.h"
#include "renderdata/GridLayout.h"

using engine::apps::kGridHeight;
using engine::apps::kGridSpacing;
using engine::apps::kGridTop;

namespace engine::bench {
    namespace {
        // The largest side of a copy in the grid: the width of a cube
        constexpr float kGridObjectSize = 2.0f;
    } // namespace

    StressGrid::StressGrid(render::GpuResources &resources, apps::GridMode mode, uint32_t count,
                           const render::Model &model)
        : m_mode(mode), m_side(apps::GridSide(count)), m_parts(model.parts),
          m_base(render::FitToCell(model.bounds, kGridObjectSize, {0.0f, kGridHeight, 0.0f})) {
        core::LogInfo("Grid: {} x {} = {} copies of {} parts, {}", m_side, m_side, ObjectCount(), m_parts.size(),
                      apps::ToString(m_mode));
        if (m_mode == apps::GridMode::Static) {
            render::RenderList gridList;
            render::SubmitGrid(gridList, m_parts, m_base, m_side, m_side, kGridSpacing, kGridSpacing);
            // gridList can go: the matrices are in GPU memory now
            m_staticList = resources.CreateStaticList(gridList, L"Grid/Instances");
        }
    }

    void StressGrid::Submit(render::RenderList &list) const {
        if (m_mode == apps::GridMode::Dynamic) {
            render::SubmitGrid(list, m_parts, m_base, m_side, m_side, kGridSpacing, kGridSpacing);
        }
    }

    float StressGrid::Extent() const noexcept {
        return m_side == 0 ? 0.0f : static_cast<float>(m_side - 1) * kGridSpacing + kGridObjectSize;
    }

    std::array<scene::CameraKeyframe, 5> StressGrid::CameraPath() const noexcept {
        // Far enough that most of the grid is in view at a 60 degree FOV; +10 for tiny grids.
        // The keyframes are the corners of a diamond, so the camera comes closer between them.
        const float radius = 0.75f * Extent() + 10.0f;
        const float height = kGridHeight + 0.4f * Extent() + 10.0f;
        const DirectX::XMFLOAT3 center{0.0f, kGridHeight, 0.0f};
        constexpr uint32_t kQuarter = kStressPathFrames / 4;
        return {{
            {.frame = 0 * kQuarter, .position = {radius, height, 0.0f}, .target = center},
            {.frame = 1 * kQuarter, .position = {0.0f, height, radius}, .target = center},
            {.frame = 2 * kQuarter, .position = {-radius, height, 0.0f}, .target = center},
            {.frame = 3 * kQuarter, .position = {0.0f, height, -radius}, .target = center},
            {.frame = kStressPathFrames, .position = {radius, height, 0.0f}, .target = center},
        }};
    }

    render::LightSet StressGrid::Lights() const {
        const float offset = 0.25f * Extent();
        const float height = kGridTop + 6.0f;
        // Down and along +Z
        constexpr DirectX::XMFLOAT3 kSpotDirection{-0.3f, -1.0f, 1.2f};
        const auto point = [](const DirectX::XMFLOAT3 &color) {
            return render::LocalLight{.type = render::LightType::Point, .color = color, .strength = 3.5f};
        };
        const auto spot = [](const DirectX::XMFLOAT3 &color) {
            return render::LocalLight{.type = render::LightType::Spot, .color = color, .strength = 1.5f};
        };

        render::LightSet lights;
        lights.points = {
            {.light = point({0.0f, 0.0f, 1.0f}), .position = {-offset, height, 0.0f}}, // blue
            {.light = point({1.0f, 0.0f, 0.0f}), .position = {offset, height, 0.0f}},  // red
        };
        lights.spots = {
            {.light = spot({1.0f, 0.55f, 0.2f}), .position = {0.0f, height, offset}, .direction = kSpotDirection},
            {.light = spot({0.0f, 1.0f, 0.0f}), .position = {0.0f, height, -offset}, .direction = kSpotDirection},
        };
        return lights;
    }

    std::span<const render::StaticListHandle> StressGrid::StaticLists() const noexcept {
        if (!m_staticList) {
            return {};
        }
        return {&*m_staticList, 1};
    }
} // namespace engine::bench
