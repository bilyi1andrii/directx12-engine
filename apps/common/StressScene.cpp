#include "StressScene.h"

#include <expected>
#include <string>
#include <vector>

#include "assets/Primitives.h"
#include "assets/TextureLoader.h"
#include "core/Error.h"
#include "core/Log.h"

namespace engine::apps {
    namespace {
        // A 256x256 light and dark grey checkerboard. Without mips, distant cubes would shimmer.
        std::expected<render::TextureData, core::Error> MakeCheckerboard() {
            constexpr uint32_t kSize = 256;
            constexpr uint32_t kSquare = 32;
            std::vector<std::byte> rgba8(size_t{kSize} * kSize * 4);
            for (uint32_t y = 0; y < kSize; ++y) {
                for (uint32_t x = 0; x < kSize; ++x) {
                    const bool light = (x / kSquare + y / kSquare) % 2 == 0;
                    const std::byte value{light ? uint8_t{220} : uint8_t{60}};
                    std::byte *pixel = &rgba8[(size_t{y} * kSize + x) * 4];
                    pixel[0] = value;
                    pixel[1] = value;
                    pixel[2] = value;
                    pixel[3] = std::byte{255};
                }
            }
            return assets::CreateTextureFromPixels(rgba8, kSize, kSize, assets::TextureUsage::Diffuse);
        }
    } // namespace

    std::optional<GridMode> ParseGridMode(std::string_view text) noexcept {
        if (text == "static") {
            return GridMode::Static;
        }
        if (text == "dynamic") {
            return GridMode::Dynamic;
        }
        return std::nullopt;
    }

    std::string_view ToString(GridMode mode) noexcept {
        return mode == GridMode::Static ? "static" : "dynamic";
    }

    uint32_t GridSide(uint32_t count) noexcept {
        // 64-bit, so the square cannot overflow
        uint32_t side = 0;
        while (uint64_t{side} * side < count) {
            ++side;
        }
        return side;
    }

    render::FrameView MakeFrameView(const scene::Camera &camera, float aspectRatio) {
        render::FrameView view{};
        DirectX::XMStoreFloat4x4(&view.view, scene::ViewMatrix(camera));
        DirectX::XMStoreFloat4x4(&view.projection, scene::ProjectionMatrix(camera, aspectRatio));
        view.cameraPosition = camera.position;
        return view;
    }

    render::TextureHandle CreateGridTexture(render::Renderer &renderer,
                                            const std::optional<std::filesystem::path> &path) {
        const std::expected<render::TextureData, core::Error> texture =
            path ? assets::LoadTexture(*path, assets::TextureUsage::Diffuse) : MakeCheckerboard();
        if (!texture) {
            core::LogWarning("Grid texture: {}. Using the grey fallback.", texture.error().message);
            return renderer.UnloadedTexture();
        }
        const std::wstring name = path ? L"Grid/" + path->filename().wstring() : L"Grid/Checkerboard";
        return renderer.CreateTexture(*texture, name);
    }

    StressGrid::StressGrid(render::Renderer &renderer, GridMode mode, uint32_t count, render::TextureHandle texture)
        : m_mode(mode), m_side(GridSide(count)),
          m_part{.mesh = renderer.CreateMesh(assets::MakeCube(), L"Cube").front(),
                 .material = {.index = texture.index},
                 .bounds = assets::kCubeBounds} {
        DirectX::XMStoreFloat4x4(&m_base, DirectX::XMMatrixTranslation(0.0f, kGridHeight, 0.0f));
        core::LogInfo("Grid: {} x {} = {} cubes, {}", m_side, m_side, ObjectCount(), ToString(m_mode));
        if (m_mode == GridMode::Static) {
            render::RenderList gridList;
            render::SubmitGrid(gridList, {&m_part, 1}, m_base, m_side, m_side, kGridSpacing, kGridSpacing);
            // gridList can go: the matrices are in GPU memory now
            m_staticList = renderer.CreateStaticList(gridList, L"Grid/Instances");
        }
    }

    void StressGrid::Submit(render::RenderList &list) const {
        if (m_mode == GridMode::Dynamic) {
            render::SubmitGrid(list, {&m_part, 1}, m_base, m_side, m_side, kGridSpacing, kGridSpacing);
        }
    }

    float StressGrid::Extent() const noexcept {
        constexpr float kCubeSize = 2.0f;
        return m_side == 0 ? 0.0f : static_cast<float>(m_side - 1) * kGridSpacing + kCubeSize;
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
} // namespace engine::apps
