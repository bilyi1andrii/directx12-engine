#ifndef D3D12_ENGINE_UIPASS_H
#define D3D12_ENGINE_UIPASS_H

#include <cstdint>

#include "render/FrameGraph.h"

namespace engine::render {
    class UiRenderer;

    // Adds the debug UI to `graph`: ImGui draws FrameInputs::ui on top of what `target` already holds, through
    // `target`'s view number `view`. Does nothing in a frame without UI draw data. `ui` must outlive the graph's
    // passes.
    void AddUiPass(FrameGraph &graph, FgTexture target, uint32_t view, UiRenderer &ui);
} // namespace engine::render

#endif // D3D12_ENGINE_UIPASS_H
