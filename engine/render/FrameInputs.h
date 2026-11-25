#ifndef D3D12_ENGINE_FRAMEINPUTS_H
#define D3D12_ENGINE_FRAMEINPUTS_H

#include <span>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RendererSettings.h"
#include "renderdata/ShadowView.h"
#include "renderdata/TransparentSort.h"
#include "rhi/RhiCommon.h"

// ImGui's per-frame draw data; opaque here
struct ImDrawData;

namespace render {
    // This frame's data, which the frame graph's passes read through PassContext::Inputs(). The renderer fills it
    // before FrameGraph::Execute. Everything it points to is valid only during that Execute.
    struct FrameInputs {
        // FrameConstants (b1) in this frame's upload ring. The graph binds it before every pass.
        D3D12_GPU_VIRTUAL_ADDRESS frameConstants = 0;
        DrawPath drawPath = DrawPath::Instanced;
        // The opaque draws: each static list's draws, then those of the moving objects (the opaque pass skips Blend)
        std::span<const std::span<const InstanceDraw>> drawSets;
        std::span<const TransparentObject> transparentObjects; // static and moving, sorted back to front
        ShadowView shadow;
        // Null when the app draws no UI this frame
        ImDrawData *ui = nullptr;
    };
} // namespace render

#endif // D3D12_ENGINE_FRAMEINPUTS_H
