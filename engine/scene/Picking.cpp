#include "scene/Picking.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace engine::scene {
    using namespace DirectX;

    Ray MakePickRay(const Camera &camera, XMFLOAT2 pointPx, XMFLOAT2 viewportPx) {
        const XMMATRIX view = ViewMatrix(camera);
        const XMMATRIX projection = ProjectionMatrix(camera, viewportPx.x / viewportPx.y);
        // Unproject: pixel + depth -> world, the inverse of what the GPU does (world -> clip -> pixel). Depth 0 is
        // the near plane and 1 the far plane, as in the projection matrix. World matrix: identity.
        const XMVECTOR nearPoint =
            XMVector3Unproject(XMVectorSet(pointPx.x, pointPx.y, 0.0f, 1.0f), 0.0f, 0.0f, viewportPx.x, viewportPx.y,
                               0.0f, 1.0f, projection, view, XMMatrixIdentity());
        const XMVECTOR farPoint =
            XMVector3Unproject(XMVectorSet(pointPx.x, pointPx.y, 1.0f, 1.0f), 0.0f, 0.0f, viewportPx.x, viewportPx.y,
                               0.0f, 1.0f, projection, view, XMMatrixIdentity());
        Ray ray;
        XMStoreFloat3(&ray.origin, nearPoint);
        XMStoreFloat3(&ray.direction, XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint)));
        return ray;
    }

    std::optional<float> IntersectRayBox(const Ray &ray, const XMFLOAT4X4 &world) {
        // Test in the box's own space, where it is axis-aligned. The world matrix is affine, so the ray maps to
        // origin' + t * direction' with the same t: the hit distance needs no conversion back.
        XMVECTOR determinant;
        const XMMATRIX toLocal = XMMatrixInverse(&determinant, XMLoadFloat4x4(&world));
        if (std::abs(XMVectorGetX(determinant)) < std::numeric_limits<float>::min()) {
            return std::nullopt; // a flattened box has no inside
        }
        XMFLOAT3 origin{};
        XMFLOAT3 direction{};
        XMStoreFloat3(&origin, XMVector3TransformCoord(XMLoadFloat3(&ray.origin), toLocal));        // a point
        XMStoreFloat3(&direction, XMVector3TransformNormal(XMLoadFloat3(&ray.direction), toLocal)); // no translation
        const std::array origins{origin.x, origin.y, origin.z};
        const std::array directions{direction.x, direction.y, direction.z};

        // Slab test: the box is where the ray is between both planes of every axis, so intersect the t intervals
        // of the three slabs [-1, 1]. Starting at 0: points behind the ray's origin do not count.
        float tEnter = 0.0f;
        float tExit = std::numeric_limits<float>::max();
        for (size_t axis = 0; axis < origins.size(); ++axis) {
            const float o = origins[axis];
            const float d = directions[axis];
            if (std::abs(d) < 1e-12f) {
                // Parallel to this slab: inside it everywhere or nowhere
                if (o < -1.0f || o > 1.0f) {
                    return std::nullopt;
                }
                continue;
            }
            const float t1 = (-1.0f - o) / d;
            const float t2 = (1.0f - o) / d;
            tEnter = std::max(tEnter, std::min(t1, t2));
            tExit = std::min(tExit, std::max(t1, t2));
            if (tEnter > tExit) {
                return std::nullopt;
            }
        }
        return tEnter;
    }
} // namespace engine::scene
