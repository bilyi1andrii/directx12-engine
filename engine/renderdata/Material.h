#ifndef D3D12_ENGINE_MATERIAL_H
#define D3D12_ENGINE_MATERIAL_H

#include <cstddef>
#include <cstdint>

#include "renderdata/Handles.h"
#include "renderdata/MaterialDesc.h"

namespace engine::render {
    enum class Shading : uint8_t {
        Lit,   // lit by the frame's lights
        Unlit, // the albedo colour as it is (light markers)
    };
    // The renderer keeps a PSO per shading
    inline constexpr size_t kShadingCount = 2;

    // What the renderer needs to draw a surface. GpuUploader::CreateMaterial turns it into a MaterialHandle.
    struct MaterialInfo {
        TextureHandle albedo; // default: the grey fallback texture
        Shading shading = Shading::Lit;
        bool doubleSided = false; // draw back faces too (leaves, curtains)

        // Only lit materials may cut holes or be transparent
        AlphaMode alphaMode = AlphaMode::Opaque;
        // Mask: a pixel whose alpha is below this is not drawn. Stored as a byte, the form the shader receives, where
        // 0 to 255 stands for 0 to 1. The default is one half.
        uint8_t alphaCutoff = 128;
        // Blend: how much of the surface covers what is behind it, as a byte like alphaCutoff. The default is fully
        // opaque. The shader multiplies it with the alpha of the albedo and the mask.
        uint8_t opacity = 255;
        // Mask and Blend: the surface's alpha is the albedo's alpha times this mask. They must set it: without a
        // separate mask, use a white texture. The default, the grey fallback, is dark enough to cut out the whole
        // surface. Not read for opaque materials.
        TextureHandle opacityMap;

        bool operator==(const MaterialInfo &) const = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MATERIAL_H
