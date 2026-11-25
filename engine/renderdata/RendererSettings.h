#ifndef D3D12_ENGINE_RENDERERSETTINGS_H
#define D3D12_ENGINE_RENDERERSETTINGS_H

// Options the app and the debug UI change at runtime: how objects are drawn, and vsync. The renderer reads them
// once per frame, so they never change halfway through one.

#include <cstdint>

namespace render {
    enum class DrawPath : uint8_t {
        Instanced,
        NonInstanced,
    };

    struct RendererSettings {
        DrawPath drawPath = DrawPath::Instanced;
        bool vsync = true;
    };
} // namespace render

#endif // D3D12_ENGINE_RENDERERSETTINGS_H
