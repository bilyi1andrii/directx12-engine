#ifndef D3D12_ENGINE_RENDERERSETTINGS_H
#define D3D12_ENGINE_RENDERERSETTINGS_H

#include <cstdint>

namespace render {
    // How the renderer draws the buckets of a RenderList. Both paths read the same instance data.
    enum class DrawPath : uint8_t {
        Instanced,    // one DrawIndexedInstanced per bucket
        NonInstanced, // one draw per object; measures per-draw cost
    };

    // Options changed at runtime by the app and the debug UI (ui::DebugUI). The renderer copies them once per
    // frame, in BeginFrame, so they never change in the middle of a frame.
    struct RendererSettings {
        DrawPath drawPath = DrawPath::Instanced;
        // Present waits for the display's next refresh. Off: presents at once (with tearing if supported), so
        // the frame rate is not capped; benchmarks run with it off.
        bool vsync = true;
    };
} // namespace render

#endif // D3D12_ENGINE_RENDERERSETTINGS_H
