#ifndef D3D12_ENGINE_HANDLES_H
#define D3D12_ENGINE_HANDLES_H

#include <cstdint>

namespace engine::render {
    // Handles name GPU objects across layers instead of pointers (plan §2.3): code above the renderer
    // stores and compares them but never sees a D3D12 resource.
    // For now a plain index into the renderer's table; AssetCache adds a generation later in step 2.

    // A mesh (vertex + index buffer) owned by the renderer
    struct MeshHandle {
        uint32_t index = 0;

        bool operator==(const MeshHandle &) const = default;
    };

    // A material. Until MaterialGPU (step 6) a material is its albedo texture: index = TextureHandle::index, which
    // reaches shaders as DrawConstants::materialIndex. The default, 0, is the grey fallback texture.
    struct MaterialHandle {
        uint32_t index = 0;

        bool operator==(const MaterialHandle &) const = default;
    };

    // A texture owned by the renderer. The index is its bindless slot (shaders read ResourceDescriptorHeap[index]),
    // so it needs no table lookup. The default, 0, is the grey "unloaded" fallback texture.
    struct TextureHandle {
        uint32_t index = 0;

        bool operator==(const TextureHandle &) const = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_HANDLES_H
