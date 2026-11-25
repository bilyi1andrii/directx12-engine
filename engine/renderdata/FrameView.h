#ifndef D3D12_ENGINE_FRAMEVIEW_H
#define D3D12_ENGINE_FRAMEVIEW_H

#include <DirectXMath.h>

namespace render {
    // Where a frame is seen from
    struct FrameView {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT3 cameraPosition; // world space
    };
} // namespace render

#endif // D3D12_ENGINE_FRAMEVIEW_H
