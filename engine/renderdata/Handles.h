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

    // A material. For now a material is just its albedo texture, so index = TextureHandle::index
    // (a material buffer comes in step 6). The default, 0, is the grey fallback texture.
    struct MaterialHandle {
        uint32_t index = 0;

        bool operator==(const MaterialHandle &) const = default;
    };

    // A texture owned by the renderer. The index is its bindless slot: shaders read ResourceDescriptorHeap[index].
    // The default, 0, is the grey fallback texture.
    struct TextureHandle {
        uint32_t index = 0;

        bool operator==(const TextureHandle &) const = default;
    };

    // Objects whose world matrices never change, uploaded once into GPU memory (DX11's static buckets).
    // Created by Renderer::CreateStaticList; the index points into the renderer's table.
    struct StaticListHandle {
        uint32_t index = 0;

        bool operator==(const StaticListHandle &) const = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_HANDLES_H
