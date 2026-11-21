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
        // Mask: a pixel whose alpha is below alphaCutoff / 255 is not drawn. 128 is about 0.5.
        uint8_t alphaCutoff = 128;
        // Mask and Blend: the alpha is the albedo's alpha times this texture's red channel. A material without a
        // separate mask uses a white texture. Not read for opaque materials.
        TextureHandle opacityMap;

        bool operator==(const MaterialInfo &) const = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MATERIAL_H
