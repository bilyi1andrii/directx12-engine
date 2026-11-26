#ifndef D3D12_ENGINE_GRIDLAYOUT_H
#define D3D12_ENGINE_GRIDLAYOUT_H

// Places copies of a model on a grid, each scaled to fit its cell, so the bench and the sandbox lay out their grids
// the same way.

#include <cstdint>

#include <DirectXMath.h>

#include "renderdata/Model.h"

namespace render {
    [[nodiscard]] inline DirectX::XMFLOAT3 GridCellOffset(uint32_t row, uint32_t col, uint32_t rows, uint32_t cols,
                                                          float spacingX, float spacingZ) noexcept {
        const float centerX = (static_cast<float>(cols) - 1.0f) * 0.5f;
        const float centerZ = (static_cast<float>(rows) - 1.0f) * 0.5f;
        return {(static_cast<float>(col) - centerX) * spacingX, 0.0f, (static_cast<float>(row) - centerZ) * spacingZ};
    }

    [[nodiscard]] DirectX::XMFLOAT4X4 FitToCell(const Aabb &bounds, float size, const DirectX::XMFLOAT3 &center);
} // namespace render

#endif // D3D12_ENGINE_GRIDLAYOUT_H
