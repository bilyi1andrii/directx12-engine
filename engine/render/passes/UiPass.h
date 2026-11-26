#ifndef D3D12_ENGINE_UIPASS_H
#define D3D12_ENGINE_UIPASS_H

// Adds the debug UI to the frame graph, drawn last, on top of the scene.

#include <cstdint>

#include "render/FrameGraph.h"

namespace render {
    class UiRenderer;

    void AddUiPass(FrameGraph &graph, FgTexture target, uint32_t view, UiRenderer &ui);
} // namespace render

#endif // D3D12_ENGINE_UIPASS_H
