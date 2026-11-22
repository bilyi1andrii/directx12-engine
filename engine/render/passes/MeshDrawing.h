#ifndef D3D12_ENGINE_MESHDRAWING_H
#define D3D12_ENGINE_MESHDRAWING_H

// Include only from engine_render's .cpp files: it needs the shader include path (common/shared_types.hlsli),
// which users of engine_render do not have.

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/Assert.h"
#include "renderdata/Material.h"
#include "renderdata/Vertex.h"
#include "rhi/BindlessHeap.h"
#include "rhi/RhiCommon.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

// What the passes that draw meshes share: the vertex layout and the per-draw root constants (b0)
namespace engine::render {
    static_assert(sizeof(gpu::DrawConstants) <= gpu::kDrawConstantCount * sizeof(uint32_t),
                  "DrawConstants must fit in the root constants of b0 (common/root_signature.hlsli)");
    // Position of DrawConstants::instanceOffset in b0, in 32-bit values
    inline constexpr uint32_t kInstanceOffsetConstant = offsetof(gpu::DrawConstants, instanceOffset) / sizeof(uint32_t);

    static_assert(rhi::BindlessHeap::kDefaultCapacity <= (1u << 24),
                  "DrawConstants::alphaInfo holds a bindless index in 24 bits");

    // The material's mask and its alpha byte, packed into one root constant (layout: DrawConstants in
    // shared_types.hlsli). The byte is the opacity for transparent materials and the cutoff for all others.
    [[nodiscard]] inline uint32_t PackAlphaInfo(const MaterialInfo &material) {
        ENGINE_ASSERT_MSG(material.opacityMap.index < (1u << 24), "Opacity map index {} does not fit in 24 bits",
                          material.opacityMap.index);
        const uint8_t alphaByte = material.alphaMode == AlphaMode::Blend ? material.opacity : material.alphaCutoff;
        return (material.opacityMap.index << 8) | alphaByte;
    }

    // How the GPU reads one Vertex (renderdata/Vertex.h): must match VSInput in vs_mesh.hlsl
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

#endif // D3D12_ENGINE_MESHDRAWING_H
