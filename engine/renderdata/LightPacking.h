#ifndef D3D12_ENGINE_LIGHTPACKING_H
#define D3D12_ENGINE_LIGHTPACKING_H

#include <cstdint>

#include "renderdata/Lights.h"

namespace engine::gpu {
    struct FrameConstants; // common/shared_types.hlsli
}

namespace engine::render {
    // Writes `lights` into the light fields of `constants`, in the form the shaders expect:
    // - only enabled point and spot lights, the first gpu::kMaxPointLights / gpu::kMaxSpotLights of them;
    // - directions at unit length (a zero direction points straight down);
    // - cone angles as cosines, with innerCos always greater than outerCos;
    // - strength 0 for a disabled directional light;
    // - shininess of at least 1.
    // The other fields of `constants` are left as they are.
    void PackLights(const LightSet &lights, gpu::FrameConstants &constants);

    // How many lights of each kind PackLights passes to the shaders: enabled ones, up to the limits
    struct PackedLightCounts {
        uint32_t directional = 0; // 0 or 1
        uint32_t points = 0;
        uint32_t spots = 0;
    };
    [[nodiscard]] PackedLightCounts CountPackedLights(const LightSet &lights);
} // namespace engine::render

#endif // D3D12_ENGINE_LIGHTPACKING_H
