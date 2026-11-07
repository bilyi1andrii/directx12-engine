#ifndef D3D12_ENGINE_STRESSSCENE_H
#define D3D12_ENGINE_STRESSSCENE_H

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

// Scene code shared by the apps (sandbox, bench)
namespace engine::apps {
    // The stress grid without --count: 20 x 20
    inline constexpr uint32_t kDefaultGridCount = 400;

    // Static: uploaded once into GPU memory (Renderer::CreateStaticList). Dynamic: rebuilt and uploaded
    // again every frame, like moving objects (and like DX11, which re-uploads every bucket).
    enum class GridMode : uint8_t { Static, Dynamic };

    // "static" or "dynamic" (the --grid values); nullopt for anything else
    [[nodiscard]] std::optional<GridMode> ParseGridMode(std::string_view text) noexcept;
    [[nodiscard]] std::string_view ToString(GridMode mode) noexcept;

    // Side of the grid for `count` cubes: the smallest n with n * n >= count (--count 50000 gives 224 x 224)
    [[nodiscard]] uint32_t GridSide(uint32_t count) noexcept;

    // The renderer's input for one frame, from the camera (scene and render do not know each other)
    [[nodiscard]] render::FrameView MakeFrameView(const scene::Camera &camera, float aspectRatio);

    // The grid's texture: the image at `path`, or a 256x256 grey checkerboard without one. If the file fails
    // to load: a warning and the grey fallback.
    [[nodiscard]] render::TextureHandle CreateGridTexture(render::Renderer &renderer,
                                                          const std::optional<std::filesystem::path> &path);

    // A square grid of textured cubes (spacing 3, 4 units below the origin), static or dynamic
    class StressGrid {
    public:
        // `count` is rounded up to a square (GridSide). Static: the grid is uploaded here, once.
        StressGrid(render::Renderer &renderer, GridMode mode, uint32_t count, render::TextureHandle texture);

        // Dynamic: adds the grid to this frame's list (after list.Clear()). Static: does nothing.
        void Submit(render::RenderList &list) const;

        // Pass to Renderer::Render every frame: the grid's static list, empty in dynamic mode
        [[nodiscard]] std::span<const render::StaticListHandle> StaticLists() const noexcept;

        [[nodiscard]] GridMode Mode() const noexcept { return m_mode; }
        [[nodiscard]] uint32_t Side() const noexcept { return m_side; }
        [[nodiscard]] size_t ObjectCount() const noexcept { return size_t{m_side} * m_side; }

    private:
        GridMode m_mode;
        uint32_t m_side;
        render::BatchKey m_key;
        DirectX::XMFLOAT4X4 m_base{};
        std::optional<render::StaticListHandle> m_staticList; // static mode only
    };
} // namespace engine::apps

#endif // D3D12_ENGINE_STRESSSCENE_H
