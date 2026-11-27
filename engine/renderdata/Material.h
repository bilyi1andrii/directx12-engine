#ifndef D3D12_ENGINE_MATERIAL_H
#define D3D12_ENGINE_MATERIAL_H

// A material: how a surface looks when it is drawn. It names the surface's texture, whether lights shade it, whether
// its back faces show, and whether it is solid, cut out or see-through. The renderer keeps each material once and
// hands out a handle to it.

#include <cstdint>

#include "renderdata/Handles.h"
#include "renderdata/MaterialDesc.h"

namespace render {
    enum class Shading : uint8_t {
        Lit,
        Unlit,
        Count,
    };

    struct MaterialInfo {
        TextureHandle albedo;
        Shading shading = Shading::Lit;
        bool doubleSided = false;

        AlphaMode alphaMode = AlphaMode::Opaque;
        uint8_t alphaCutoff = 128; // 0-255
        uint8_t opacity = 255; // 0-255, for Blend

        TextureHandle opacityMap;

        bool operator==(const MaterialInfo &) const = default;
    };
} // namespace render

#endif // D3D12_ENGINE_MATERIAL_H