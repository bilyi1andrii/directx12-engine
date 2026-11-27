#ifndef D3D12_ENGINE_HANDLES_H
#define D3D12_ENGINE_HANDLES_H

// Small typed indices that name the renderer's meshes, materials, textures and static lists, so code above the
// renderer never holds a GPU resource.

#include <cstdint>

namespace render {
    // tag only makes each handle its own type
    template <typename Tag> struct Handle {
        uint32_t index = 0; // into renderer's table or bindless slot for textures

        bool operator==(const Handle &) const = default;
    };

    using MeshHandle = Handle<struct MeshTag>;
    using MaterialHandle = Handle<struct MaterialTag>;
    using TextureHandle = Handle<struct TextureTag>; // bindless
    using StaticListHandle = Handle<struct StaticListTag>;
} // namespace render

#endif // D3D12_ENGINE_HANDLES_H
