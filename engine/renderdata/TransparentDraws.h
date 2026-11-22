#ifndef D3D12_ENGINE_TRANSPARENTDRAWS_H
#define D3D12_ENGINE_TRANSPARENTDRAWS_H

#include <cstdint>
#include <span>

#include <DirectXMath.h>

#include "renderdata/RenderList.h"

namespace engine::render {
    // One object with a transparent material. Transparent objects are drawn one at a time, after the solid ones, so
    // each one blends over what is already behind it.
    struct TransparentDraw {
        BatchKey key;
        uint32_t instanceBufferIndex = 0; // bindless index of the instance buffer that holds the object's world matrix
        uint32_t element = 0;             // the object's element in that buffer
        float viewDepth = 0.0f;           // ViewDepth of the object's position
    };

    // How far `position` (world space) lies in front of the camera, measured along the direction it looks: the
    // view-space z. Negative behind the camera. `view` is world -> view, row-vector convention (FrameView::view).
    [[nodiscard]] float ViewDepth(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT3 &position) noexcept;

    // Orders `draws` back to front: the farthest first. Draws at the same depth are ordered by instance buffer, then
    // element, so their order does not change from frame to frame.
    void SortBackToFront(std::span<TransparentDraw> draws);
} // namespace engine::render

#endif // D3D12_ENGINE_TRANSPARENTDRAWS_H
