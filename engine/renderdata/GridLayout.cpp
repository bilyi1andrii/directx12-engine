#include "renderdata/GridLayout.h"

#include <algorithm>

namespace engine::render {
    DirectX::XMFLOAT4X4 FitToCell(const Aabb &bounds, float size, const DirectX::XMFLOAT3 &center) {
        using namespace DirectX;

        const float largest =
            std::max({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z});
        const float scale = largest > 0.0f ? size / largest : 1.0f;
        const XMMATRIX toOrigin = XMMatrixTranslation(-(bounds.min.x + bounds.max.x) * 0.5f, //
                                                      -(bounds.min.y + bounds.max.y) * 0.5f, //
                                                      -(bounds.min.z + bounds.max.z) * 0.5f);

        XMFLOAT4X4 result{};
        XMStoreFloat4x4(&result, toOrigin * XMMatrixScaling(scale, scale, scale) *
                                     XMMatrixTranslation(center.x, center.y, center.z));
        return result;
    }
} // namespace engine::render
