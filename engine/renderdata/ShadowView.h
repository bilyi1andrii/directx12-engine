#ifndef D3D12_ENGINE_SHADOWVIEW_H
#define D3D12_ENGINE_SHADOWVIEW_H

#include <cstdint>

#include <DirectXMath.h>

#include "renderdata/Model.h"

namespace render {
    inline constexpr uint32_t kShadowMapSize = 2048;

    struct ShadowView {
        DirectX::XMFLOAT4X4 viewProjection{};
        float texelWorldSize = 0.0f;
    };

    [[nodiscard]] ShadowView MakeShadowView(const DirectX::XMFLOAT3 &lightDirection, const Aabb &bounds,
                                            uint32_t mapSize = kShadowMapSize);

    [[nodiscard]] Aabb TransformBox(const Aabb &local, const DirectX::XMFLOAT4X4 &world);

    [[nodiscard]] Aabb Union(const Aabb &a, const Aabb &b);
} // namespace render

#endif // D3D12_ENGINE_SHADOWVIEW_H
