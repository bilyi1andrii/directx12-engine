#ifndef D3D12_ENGINE_VERTEX_H
#define D3D12_ENGINE_VERTEX_H

// The vertex every mesh uses. Its layout must match the shaders' vertex input and the renderer's input layout.

#include <DirectXMath.h>

namespace render {
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 tangent;
    };
    static_assert(sizeof(Vertex) == 44, "Vertex must keep the DX11 layout: 44 bytes, no padding");
} // namespace render

#endif // D3D12_ENGINE_VERTEX_H
