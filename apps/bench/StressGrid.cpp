#include "StressGrid.h"

#include "core/Log.h"

using engine::apps::kGridHeight;
using engine::apps::kGridSpacing;

namespace engine::bench {
    namespace {
        // The largest side of a copy in the grid: the width of a cube
        constexpr float kGridObjectSize = 2.0f;
    } // namespace

    StressGrid::StressGrid(render::Renderer &renderer, apps::GridMode mode, uint32_t count, const render::Model &model)
        : m_mode(mode), m_side(apps::GridSide(count)), m_parts(model.parts),
          m_base(render::FitToCell(model.bounds, kGridObjectSize, {0.0f, kGridHeight, 0.0f})) {
        core::LogInfo("Grid: {} x {} = {} copies of {} parts, {}", m_side, m_side, ObjectCount(), m_parts.size(),
                      apps::ToString(m_mode));
        if (m_mode == apps::GridMode::Static) {
            render::RenderList gridList;
            render::SubmitGrid(gridList, m_parts, m_base, m_side, m_side, kGridSpacing, kGridSpacing);
            // gridList can go: the matrices are in GPU memory now
            m_staticList = renderer.CreateStaticList(gridList, L"Grid/Instances");
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
        const DirectX::XMFLOAT3 centre{0.0f, kGridHeight, 0.0f};
        constexpr uint32_t kQuarter = kStressPathFrames / 4;
        return {{
            {.frame = 0 * kQuarter, .position = {radius, height, 0.0f}, .target = centre},
            {.frame = 1 * kQuarter, .position = {0.0f, height, radius}, .target = centre},
            {.frame = 2 * kQuarter, .position = {-radius, height, 0.0f}, .target = centre},
            {.frame = 3 * kQuarter, .position = {0.0f, height, -radius}, .target = centre},
            {.frame = kStressPathFrames, .position = {radius, height, 0.0f}, .target = centre},
        }};
    }

    std::span<const render::StaticListHandle> StressGrid::StaticLists() const noexcept {
        if (!m_staticList) {
            return {};
        }
        return {&*m_staticList, 1};
    }
} // namespace engine::bench
