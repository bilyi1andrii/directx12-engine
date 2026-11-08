#ifndef D3D12_ENGINE_STRESSSCENE_H
#define D3D12_ENGINE_STRESSSCENE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

#include <DirectXMath.h>

#include "render/Renderer.h"
#include "renderdata/Handles.h"
#include "renderdata/RenderList.h"
#include "scene/Camera.h"
#include "scene/CameraPath.h"

// The cube grid's layout and options, shared by the sandbox and the bench, and the bench's StressGrid
namespace engine::apps {
    // Length of the grid's camera path in frames, the same as the bench's default --frames
    inline constexpr uint32_t kStressPathFrames = 1000;

    // 20 x 20 cubes
    inline constexpr uint32_t kDefaultGridCount = 400;

    // Cubes 2 units wide with 1 unit between them, their centres 4 units below the origin
    inline constexpr float kGridSpacing = 3.0f;
    inline constexpr float kGridHeight = -4.0f;
    // The top of the grid's cubes, where models stand
    inline constexpr float kGridTop = kGridHeight + 1.0f;

    // Static: uploaded to GPU memory once. Dynamic: rebuilt and uploaded every frame, like moving objects.
    enum class GridMode : uint8_t { Static, Dynamic };

    // "static" or "dynamic"; nullopt for anything else
    [[nodiscard]] std::optional<GridMode> ParseGridMode(std::string_view text) noexcept;
    [[nodiscard]] std::string_view ToString(GridMode mode) noexcept;

    // The smallest n with n * n >= count: --count 50000 gives a 224 x 224 grid
    [[nodiscard]] uint32_t GridSide(uint32_t count) noexcept;

    // What the renderer needs from the camera (render does not know scene)
    [[nodiscard]] render::FrameView MakeFrameView(const scene::Camera &camera, float aspectRatio);

    // The image at `path`, or a grey checkerboard. A file that fails to load gives a warning and plain grey.
    [[nodiscard]] render::TextureHandle CreateGridTexture(render::Renderer &renderer,
                                                          const std::optional<std::filesystem::path> &path);

    // A square grid of textured cubes 4 units below the origin
    class StressGrid {
    public:
        // `count` is rounded up to a square. Uploads the cube mesh, and a static grid.
        StressGrid(render::Renderer &renderer, GridMode mode, uint32_t count, render::TextureHandle texture);

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

    private:
        GridMode m_mode;
        uint32_t m_side;
        render::BatchKey m_key;
        DirectX::XMFLOAT4X4 m_base{};
        std::optional<render::StaticListHandle> m_staticList; // static mode only
    };
} // namespace engine::apps

#endif // D3D12_ENGINE_STRESSSCENE_H
