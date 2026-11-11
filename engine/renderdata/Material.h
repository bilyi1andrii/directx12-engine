#ifndef D3D12_ENGINE_MATERIAL_H
#define D3D12_ENGINE_MATERIAL_H

#include <cstddef>
#include <cstdint>

#include "renderdata/Handles.h"

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

        bool operator==(const MaterialInfo &) const = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MATERIAL_H
