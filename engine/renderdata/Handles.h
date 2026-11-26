#ifndef D3D12_ENGINE_HANDLES_H
#define D3D12_ENGINE_HANDLES_H

#include <cstdint>

namespace render {
    // Handles name GPU objects across layers instead of pointers: code above the renderer
    // stores and compares them but never sees a D3D12 resource.
    // For now a plain index into the renderer's table, with no generation to detect a stale handle.
    //
    // `Tag` only makes each kind of handle its own type, so a mesh handle cannot be passed as a texture.
    // Still an aggregate: Handle{.index = i} works. What the default index 0 means depends on the kind.
    template <typename Tag> struct Handle {
        uint32_t index = 0;

        bool operator==(const Handle &) const = default;
    };

    // A mesh (vertex + index buffer) owned by the renderer. The default, 0, is the first mesh created.
    using MeshHandle = Handle<struct MeshTag>;

    // A material in the renderer's table, made by GpuUploader::CreateMaterial from a MaterialInfo (Material.h).
    // The default, 0, is the grey fallback texture, lit and one-sided.
    using MaterialHandle = Handle<struct MaterialTag>;

    // A texture owned by the renderer. The index is its bindless slot: shaders read ResourceDescriptorHeap[index].
    // The default, 0, is the grey fallback texture.
    using TextureHandle = Handle<struct TextureTag>;

    // Objects that never move, uploaded once into GPU memory by GpuResources::CreateStaticList.
    // The index points into the renderer's table.
    using StaticListHandle = Handle<struct StaticListTag>;
} // namespace render

#endif // D3D12_ENGINE_HANDLES_H
