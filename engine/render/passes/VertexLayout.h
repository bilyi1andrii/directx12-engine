#ifndef D3D12_ENGINE_VERTEXLAYOUT_H
#define D3D12_ENGINE_VERTEXLAYOUT_H

// How the GPU reads a mesh vertex: the input layout of every pipeline that draws meshes with vs_mesh. It must match
// the vertex struct and the shader's input.

#include <array>
#include <cstddef>

#include "renderdata/Vertex.h"
#include "rhi/RhiCommon.h"

namespace engine::render {
    inline constexpr std::array kVertexLayout{
        D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = "POSITION",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = offsetof(Vertex, position),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
        D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = "TEXCOORD",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = offsetof(Vertex, uv),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
        D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = "NORMAL",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = offsetof(Vertex, normal),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
        D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = "TANGENT",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = offsetof(Vertex, tangent),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
    };
} // namespace engine::render

#endif // D3D12_ENGINE_VERTEXLAYOUT_H
