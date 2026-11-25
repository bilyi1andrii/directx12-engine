#ifndef D3D12_ENGINE_DEPTHFORMAT_H
#define D3D12_ENGINE_DEPTHFORMAT_H

// The depth format and the reversed depth convention (near is 1, far is 0) that every depth texture and pipeline
// uses. Reversed depth keeps more precision far from the camera.

#include "rhi/RhiCommon.h"

namespace rhi {
    inline constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

    inline constexpr float kClearDepth = 0.0f;
    inline constexpr D3D12_COMPARISON_FUNC kDepthComparison = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
} // namespace rhi

#endif // D3D12_ENGINE_DEPTHFORMAT_H
