#ifndef D3D12_ENGINE_TRANSPARENTSORT_H
#define D3D12_ENGINE_TRANSPARENTSORT_H

// Puts see-through objects in the order they are drawn: from the farthest to the nearest. Each object is measured by
// how far the middle of its mesh lies in front of the camera. Drawn in that order after the solid surfaces, each one
// blends over what is already behind it.

#include <cstdint>
#include <span>

#include <DirectXMath.h>

#include "renderdata/RenderList.h"

namespace render {
    struct TransparentObject {
        BatchKey key;
        uint32_t instanceBufferIndex = 0;
        uint32_t element = 0;
        float viewDepth = 0.0f;
    };

    [[nodiscard]] DirectX::XMFLOAT3 SortPoint(const DirectX::XMFLOAT4X4 &world,
                                              const DirectX::XMFLOAT3 &meshCenter) noexcept;
    [[nodiscard]] float ViewDepth(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT3 &position) noexcept;
    void SortBackToFront(std::span<TransparentObject> objects);
} // namespace render

#endif // D3D12_ENGINE_TRANSPARENTSORT_H