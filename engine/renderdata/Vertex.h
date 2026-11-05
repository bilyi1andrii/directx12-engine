#ifndef D3D12_ENGINE_VERTEX_H
#define D3D12_ENGINE_VERTEX_H

#include <DirectXMath.h>

namespace engine::render {
    // The mesh vertex: what the model importer produces and the renderer's vertex buffers hold.
    // Same layout as DX11 Graphics/Vertex.h.
    //
    // Shared with HLSL, but not through common/shared_types.hlsli (a recorded exception to that rule): shaders
    // receive it through the input assembler, by semantic (VSInput in vs_mesh.hlsl), not as memory they index.
    // Its layout is described to D3D12 by kVertexLayout in Renderer.cpp. Keep the three in step.
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 tangent; // for normal mapping later
    };
    static_assert(sizeof(Vertex) == 44, "Vertex must keep the DX11 layout: 44 bytes, no padding");
} // namespace engine::render

#endif // D3D12_ENGINE_VERTEX_H
