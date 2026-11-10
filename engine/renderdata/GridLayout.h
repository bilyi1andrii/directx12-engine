#ifndef D3D12_ENGINE_GRIDLAYOUT_H
#define D3D12_ENGINE_GRIDLAYOUT_H

#include <cstdint>

#include <DirectXMath.h>

#include "renderdata/Model.h"

namespace engine::render {
    // Offset of cell (row, col) from the center of a rows x cols grid on the XZ plane (y is 0): columns go along X,
    // rows along Z. The bench's SubmitGrid and the sandbox's grid both use it, so their layouts match.
    [[nodiscard]] inline DirectX::XMFLOAT3 GridCellOffset(uint32_t row, uint32_t col, uint32_t rows, uint32_t cols,
                                                          float spacingX, float spacingZ) noexcept {
        const float centerX = (static_cast<float>(cols) - 1.0f) * 0.5f;
        const float centerZ = (static_cast<float>(rows) - 1.0f) * 0.5f;
        return {(static_cast<float>(col) - centerX) * spacingX, 0.0f, (static_cast<float>(row) - centerZ) * spacingZ};
    }

    // Model -> world for a model with these bounds: scales it so its largest side is `size` and puts the center
    // of its box at `center`. A model with no size keeps its scale.
    [[nodiscard]] DirectX::XMFLOAT4X4 FitToCell(const Aabb &bounds, float size, const DirectX::XMFLOAT3 &center);
} // namespace engine::render

#endif // D3D12_ENGINE_GRIDLAYOUT_H
