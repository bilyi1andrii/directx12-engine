#ifndef D3D12_ENGINE_RENDERERSETTINGS_H
#define D3D12_ENGINE_RENDERERSETTINGS_H

#include <cstdint>

namespace engine::render {
    // How the renderer draws the buckets of a RenderList. Both paths read the same instance data.
    enum class DrawPath : uint8_t {
        Instanced,    // one DrawIndexedInstanced per bucket
        NonInstanced, // one draw per object (DX11 RenderMode::NonInstanced); measures per-draw cost
    };

    // Options changed at runtime by the app (later by the debug UI). The renderer copies them once per
    // frame, in BeginFrame, so they never change in the middle of a frame.
    struct RendererSettings {
        DrawPath drawPath = DrawPath::Instanced;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERERSETTINGS_H
