#include "scene/Picking.h"

#include <cmath>

#include "core/Assert.h"

namespace engine::scene {
    Ray ScreenRay(const Camera &camera, float x, float y, float width, float height) {
        using namespace DirectX;
        ENGINE_ASSERT_MSG(width > 0.0f && height > 0.0f, "ScreenRay on a {} x {} view", width, height);

        // Pixels -> normalized device coordinates: x from -1 (left) to 1 (right), y from 1 (top) to -1 (bottom)
        const float ndcX = 2.0f * x / width - 1.0f;
        const float ndcY = 1.0f - 2.0f * y / height;

        // Back from clip space to world space. Depth 0.5 lies between the near and far planes, so the point is in
        // front of the camera, and the ray from the camera through it passes under (x, y).
        const XMMATRIX viewProjection = ViewMatrix(camera) * ProjectionMatrix(camera, width / height);
        const XMMATRIX clipToWorld = XMMatrixInverse(nullptr, viewProjection);
        const XMVECTOR pointInWorld = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.5f, 1.0f), clipToWorld);
        const XMVECTOR origin = XMLoadFloat3(&camera.position);

        Ray ray{};
        XMStoreFloat3(&ray.origin, origin);
        XMStoreFloat3(&ray.direction, XMVector3Normalize(XMVectorSubtract(pointInWorld, origin)));
        return ray;
    }

    std::optional<DirectX::XMFLOAT3> IntersectHorizontalPlane(const Ray &ray, float planeY) {
        constexpr float kParallel = 1e-6f;
        if (std::abs(ray.direction.y) < kParallel) {
            return std::nullopt;
        }
        // origin.y + t * direction.y = planeY
        const float t = (planeY - ray.origin.y) / ray.direction.y;
        if (t < 0.0f) {
            return std::nullopt; // the plane is behind the ray
        }
        return DirectX::XMFLOAT3{ray.origin.x + t * ray.direction.x, planeY, ray.origin.z + t * ray.direction.z};
    }
} // namespace engine::scene
