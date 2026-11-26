#ifndef D3D12_ENGINE_GRIDSETUP_H
#define D3D12_ENGINE_GRIDSETUP_H

// The cube grid's settings that the sandbox and the bench share: how many cubes, their spacing and their height.

#include <cstdint>
#include <optional>
#include <string_view>

namespace apps {
    inline constexpr uint32_t kDefaultGridCount = 400;

    inline constexpr float kGridSpacing = 3.0f;
    inline constexpr float kGridHeight = -4.0f;
    inline constexpr float kGridTop = kGridHeight + 1.0f;

    enum class GridMode : uint8_t { Static, Dynamic };

    [[nodiscard]] std::optional<GridMode> ParseGridMode(std::string_view text) noexcept;
    [[nodiscard]] std::string_view ToString(GridMode mode) noexcept;

    [[nodiscard]] uint32_t GridSide(uint32_t count) noexcept;
} // namespace apps

#endif // D3D12_ENGINE_GRIDSETUP_H
