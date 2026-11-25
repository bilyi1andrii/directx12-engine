#ifndef D3D12_ENGINE_LIGHTPACKING_H
#define D3D12_ENGINE_LIGHTPACKING_H

// Writes the scene's lights into the constants the shaders read, 
// within the shaders' limits and in the form their maths expects.

#include <cstdint>

#include "renderdata/Lights.h"

namespace gpu {
    struct FrameConstants;
}

namespace render {
    void PackLights(const LightSet &lights, gpu::FrameConstants &constants);

    struct PackedLightCounts {
        uint32_t directional = 0;
        uint32_t points = 0;
        uint32_t spots = 0;
    };
    [[nodiscard]] PackedLightCounts CountPackedLights(const LightSet &lights);
} // namespace render

#endif // D3D12_ENGINE_LIGHTPACKING_H
