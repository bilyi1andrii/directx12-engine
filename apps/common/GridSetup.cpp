#include "GridSetup.h"

namespace apps {
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
} // namespace apps
