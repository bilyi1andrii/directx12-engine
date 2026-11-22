#include "renderdata/TransparentDraws.h"

#include <algorithm>

namespace engine::render {
    DirectX::XMFLOAT3 SortPoint(const DirectX::XMFLOAT4X4 &world, const DirectX::XMFLOAT3 &meshCenter) noexcept {
        using namespace DirectX;
        XMFLOAT3 point{};
        XMStoreFloat3(&point, XMVector3TransformCoord(XMLoadFloat3(&meshCenter), XMLoadFloat4x4(&world)));
        return point;
    }

    float ViewDepth(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT3 &position) noexcept {
        // Row vectors: view-space z is the point times the third column of `view`
        return position.x * view._13 + position.y * view._23 + position.z * view._33 + view._43;
    }

    void SortBackToFront(std::span<TransparentDraw> draws) {
        // The depths were computed once, before the sort, so comparing two draws costs only a few compares
        std::ranges::sort(draws, [](const TransparentDraw &a, const TransparentDraw &b) {
            if (a.viewDepth != b.viewDepth) {
                return a.viewDepth > b.viewDepth;
            }
            if (a.instanceBufferIndex != b.instanceBufferIndex) {
                return a.instanceBufferIndex < b.instanceBufferIndex;
            }
            return a.element < b.element;
        });
    }
} // namespace engine::render
