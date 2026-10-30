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

    // A material. There is no material data yet: every draw uses material 0 until textures arrive.
    struct MaterialHandle {
        uint32_t index = 0;

        bool operator==(const MaterialHandle &) const = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_HANDLES_H
