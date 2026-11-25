#ifndef D3D12_ENGINE_VERTEX_H
#define D3D12_ENGINE_VERTEX_H

#include <DirectXMath.h>

namespace engine::render {
    // A mesh vertex, with the same layout as in the DX11 engine.
    // Must match VSInput in vs_mesh.hlsl and kVertexLayout in render/passes/VertexLayout.h.
    // It is not in shared_types.hlsli because shaders receive it through the input layout, not by reading memory.
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 tangent; // for normal mapping later
    };
    static_assert(sizeof(Vertex) == 44, "Vertex must keep the DX11 layout: 44 bytes, no padding");
} // namespace engine::render

#endif // D3D12_ENGINE_VERTEX_H
