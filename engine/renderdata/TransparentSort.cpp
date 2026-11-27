#include "renderdata/TransparentSort.h"

#include <algorithm>

namespace render {
    DirectX::XMFLOAT3 SortPoint(const DirectX::XMFLOAT4X4 &world, const DirectX::XMFLOAT3 &meshCenter) noexcept {
        using namespace DirectX;
        XMFLOAT3 point{};
        XMStoreFloat3(&point, XMVector3TransformCoord(XMLoadFloat3(&meshCenter), XMLoadFloat4x4(&world)));
        return point;
    }

    float ViewDepth(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT3 &position) noexcept {
        // only z of view transform
        return position.x * view._13 + position.y * view._23 + position.z * view._33 + view._43;
    }

    void SortBackToFront(std::span<TransparentObject> objects) {
        std::ranges::sort(objects, [](const TransparentObject &a, const TransparentObject &b) {
            if (a.viewDepth != b.viewDepth) {
                return a.viewDepth > b.viewDepth;
            }

            // ties broken by buffer and element, so order is stable across frames
            if (a.instanceBufferIndex != b.instanceBufferIndex) {
                return a.instanceBufferIndex < b.instanceBufferIndex;
            }
            return a.element < b.element;
        });
    }
} // namespace render
