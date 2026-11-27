#ifndef D3D12_ENGINE_FRAMEINPUTS_H
#define D3D12_ENGINE_FRAMEINPUTS_H

// This frame's data that the frame graph's passes read: the frame constants, the draws, the transparent objects,
// the shadow view and the UI. It is valid only while the graph executes.

#include <span>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RendererSettings.h"
#include "renderdata/ShadowView.h"
#include "renderdata/TransparentSort.h"
#include "rhi/RhiCommon.h"

struct ImDrawData;

namespace render {
    struct FrameInputs {
        D3D12_GPU_VIRTUAL_ADDRESS frameConstants = 0;
        DrawPath drawPath = DrawPath::Instanced;
        std::span<const std::span<const InstanceDraw>> drawSets; // static lists then moving objects
        std::span<const TransparentObject> transparentObjects;
        ShadowView shadow;
        ImDrawData *ui = nullptr;
    };
} // namespace render

#endif // D3D12_ENGINE_FRAMEINPUTS_H
