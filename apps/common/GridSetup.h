#ifndef D3D12_ENGINE_GRIDSETUP_H
#define D3D12_ENGINE_GRIDSETUP_H

#include <cstdint>
#include <optional>
#include <string_view>

#include "render/Renderer.h"
#include "scene/Camera.h"

// Cube-grid settings shared by the sandbox and the bench
namespace engine::apps {
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
} // namespace engine::apps

#endif // D3D12_ENGINE_GRIDSETUP_H
