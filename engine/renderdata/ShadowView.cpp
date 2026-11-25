#include "renderdata/ShadowView.h"

#include <algorithm>
#include <cmath>

namespace engine::render {
    using namespace DirectX;

    ShadowView MakeShadowView(const XMFLOAT3 &lightDirection, const Aabb &bounds, uint32_t mapSize) {
        const XMVECTOR minCorner = XMLoadFloat3(&bounds.min);
        const XMVECTOR maxCorner = XMLoadFloat3(&bounds.max);
        const XMVECTOR center = XMVectorScale(XMVectorAdd(minCorner, maxCorner), 0.5f);
        const float radius =
            std::max(0.5f * XMVectorGetX(XMVector3Length(XMVectorSubtract(maxCorner, minCorner))), 1.0f);

        XMVECTOR direction = XMVector3Normalize(XMLoadFloat3(&lightDirection));
        if (XMVector3Equal(direction, XMVectorZero()) || XMVector3IsNaN(direction)) {
            direction = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
        }
        const XMVECTOR up = std::abs(XMVectorGetY(direction)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                                                                      : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMVECTOR eye = XMVectorSubtract(center, XMVectorScale(direction, radius));
        const XMMATRIX view = XMMatrixLookToLH(eye, direction, up);
        const XMMATRIX projection = XMMatrixOrthographicLH(2.0f * radius, 2.0f * radius, 2.0f * radius, 0.0f);

        ShadowView shadow;
        XMStoreFloat4x4(&shadow.viewProjection, view * projection);
        shadow.texelWorldSize = 2.0f * radius / static_cast<float>(mapSize);
        return shadow;
    }

    Aabb TransformBox(const Aabb &local, const XMFLOAT4X4 &world) {
        const XMVECTOR center = XMVectorScale(XMVectorAdd(XMLoadFloat3(&local.min), XMLoadFloat3(&local.max)), 0.5f);
        const XMVECTOR extent =
            XMVectorScale(XMVectorSubtract(XMLoadFloat3(&local.max), XMLoadFloat3(&local.min)), 0.5f);
        const XMMATRIX matrix = XMLoadFloat4x4(&world);
        const XMVECTOR worldCenter = XMVector3TransformCoord(center, matrix);
        const XMMATRIX absolute(XMVectorAbs(matrix.r[0]), XMVectorAbs(matrix.r[1]), XMVectorAbs(matrix.r[2]),
                                XMVectorZero());
        const XMVECTOR worldExtent = XMVector3TransformNormal(extent, absolute);

        Aabb box;
        XMStoreFloat3(&box.min, XMVectorSubtract(worldCenter, worldExtent));
        XMStoreFloat3(&box.max, XMVectorAdd(worldCenter, worldExtent));
        return box;
    }

    Aabb Union(const Aabb &a, const Aabb &b) {
        Aabb box;
        XMStoreFloat3(&box.min, XMVectorMin(XMLoadFloat3(&a.min), XMLoadFloat3(&b.min)));
        XMStoreFloat3(&box.max, XMVectorMax(XMLoadFloat3(&a.max), XMLoadFloat3(&b.max)));
        return box;
    }
} // namespace engine::render
