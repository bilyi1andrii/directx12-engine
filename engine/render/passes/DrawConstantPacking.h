#ifndef D3D12_ENGINE_DRAWCONSTANTPACKING_H
#define D3D12_ENGINE_DRAWCONSTANTPACKING_H

// Packs a draw's values into the per-draw constants the mesh shaders read.

#include <cstddef>
#include <cstdint>

#include "renderdata/Material.h"
#include "rhi/BindlessHeap.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace render {
    static_assert(sizeof(gpu::DrawConstants) <= gpu::kDrawConstantCount * sizeof(uint32_t),
                  "DrawConstants must fit in the root constants of b0 (common/root_signature.hlsli)");
    // instanceOffset's slot in b0, non-instanced path rewrites it
    inline constexpr uint32_t kInstanceOffsetConstant = offsetof(gpu::DrawConstants, instanceOffset) / sizeof(uint32_t);

    static_assert(rhi::BindlessHeap::kDefaultCapacity <= (1u << 24),
                  "DrawConstants::alphaInfo holds a bindless index in 24 bits");

    [[nodiscard]] inline uint32_t PackAlphaInfo(const MaterialInfo &material) {
        const uint8_t alphaByte = material.alphaMode == AlphaMode::Blend ? material.opacity : material.alphaCutoff;
        return (material.opacityMap.index << 8) | alphaByte;
    }
} // namespace render

#endif // D3D12_ENGINE_DRAWCONSTANTPACKING_H
