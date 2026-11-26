#ifndef D3D12_ENGINE_DEPTHFORMAT_H
#define D3D12_ENGINE_DEPTHFORMAT_H

#include "rhi/RhiCommon.h"

namespace rhi {
    // The scene's depth texture: 32-bit float depth, no stencil. PSOs that draw into it take this as DSVFormat.
    inline constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

    // Depth is reversed: 1 at the near plane, 0 at the far plane. Reversed depth keeps more precision far away,
    // because floats are densest near 0.
    // The far plane: every depth texture is cleared to it.
    inline constexpr float kClearDepth = 0.0f;
    // A nearer pixel has the greater depth, so it passes the test
    inline constexpr D3D12_COMPARISON_FUNC kDepthComparison = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
} // namespace rhi

#endif // D3D12_ENGINE_DEPTHFORMAT_H
