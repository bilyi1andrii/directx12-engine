#ifndef D3D12_ENGINE_LIGHTPACKING_H
#define D3D12_ENGINE_LIGHTPACKING_H

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
} // namespace engine::render

#endif // D3D12_ENGINE_LIGHTPACKING_H
