#include "scene/Picking.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "core/Assert.h"

namespace engine::scene {
    using namespace DirectX;

    Ray MakePickRay(const Camera &camera, XMFLOAT2 pointPx, XMFLOAT2 viewportPx) {
        ENGINE_ASSERT_MSG(viewportPx.x > 0.0f && viewportPx.y > 0.0f, "MakePickRay on a {} x {} view", viewportPx.x,
                          viewportPx.y);
        const XMMATRIX view = ViewMatrix(camera);
        const XMMATRIX projection = ProjectionMatrix(camera, viewportPx.x / viewportPx.y);
        // Unproject: pixel + depth -> world, the inverse of what the GPU does (world -> clip -> pixel). Depth is
        // reversed, as in the projection matrix: 1 is the near plane and 0 the far plane. World matrix: identity.
        const XMVECTOR nearPoint =
            XMVector3Unproject(XMVectorSet(pointPx.x, pointPx.y, 1.0f, 1.0f), 0.0f, 0.0f, viewportPx.x, viewportPx.y,
                               0.0f, 1.0f, projection, view, XMMatrixIdentity());
        const XMVECTOR farPoint =
            XMVector3Unproject(XMVectorSet(pointPx.x, pointPx.y, 0.0f, 1.0f), 0.0f, 0.0f, viewportPx.x, viewportPx.y,
                               0.0f, 1.0f, projection, view, XMMatrixIdentity());
        Ray ray;
        XMStoreFloat3(&ray.origin, nearPoint);
        XMStoreFloat3(&ray.direction, XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint)));
        return ray;
    }

    std::optional<float> IntersectRayBox(const Ray &ray, const XMFLOAT4X4 &world, const render::Aabb &box) {
        // Test in the box's own space, where it is axis-aligned. The world matrix is affine, so the ray maps to
        // origin' + t * direction' with the same t: the hit distance needs no conversion back.
        XMVECTOR determinant;
        const XMMATRIX toLocal = XMMatrixInverse(&determinant, XMLoadFloat4x4(&world));
        if (std::abs(XMVectorGetX(determinant)) < std::numeric_limits<float>::min()) {
            return std::nullopt; // a scale of 0 cannot be inverted
        }
        XMFLOAT3 origin{};
        XMFLOAT3 direction{};
        XMStoreFloat3(&origin, XMVector3TransformCoord(XMLoadFloat3(&ray.origin), toLocal));        // a point
        XMStoreFloat3(&direction, XMVector3TransformNormal(XMLoadFloat3(&ray.direction), toLocal)); // no translation
        const std::array origins{origin.x, origin.y, origin.z};
        const std::array directions{direction.x, direction.y, direction.z};
        const std::array lows{box.min.x, box.min.y, box.min.z};
        const std::array highs{box.max.x, box.max.y, box.max.z};

        // Slab test: the box is where the ray is between both planes of every axis, so intersect the t intervals
        // of the three slabs [low, high]. Starting at 0 ignores everything behind the ray.
        float tEnter = 0.0f;
        float tExit = std::numeric_limits<float>::max();
        for (size_t axis = 0; axis < origins.size(); ++axis) {
            const float o = origins[axis];
            const float d = directions[axis];
            if (std::abs(d) < 1e-12f) {
                // Parallel to this slab: inside it everywhere or nowhere
                if (o < lows[axis] || o > highs[axis]) {
                    return std::nullopt;
                }
                continue;
            }
            const float t1 = (lows[axis] - o) / d;
            const float t2 = (highs[axis] - o) / d;
            tEnter = std::max(tEnter, std::min(t1, t2));
            tExit = std::min(tExit, std::max(t1, t2));
            if (tEnter > tExit) {
                return std::nullopt;
            }
        }
        return tEnter;
    }

    std::optional<ObjectId> Pick(const Scene &scene, const Ray &ray) {
        const std::span<const SceneObject> objects = scene.Objects();
        const std::span<const Transform> transforms = scene.Transforms();
        const std::span<const EntityPart> parts = scene.Parts();

        std::optional<ObjectId> nearest;
        float nearestDistance = std::numeric_limits<float>::max();
        for (uint32_t index = 0; index < objects.size(); ++index) {
            const SceneObject &object = objects[index];
            for (uint32_t i = object.firstEntity; i < object.firstEntity + object.entityCount; ++i) {
                const std::optional<float> distance = IntersectRayBox(ray, transforms[i].world, parts[i].bounds);
                // Distance 0 means the ray starts inside this box: skip it
                if (distance && *distance > 0.0f && *distance < nearestDistance) {
                    nearest = ObjectId{.index = index};
                    nearestDistance = *distance;
                }
            }
        }
        return nearest;
    }

    std::optional<XMFLOAT3> IntersectHorizontalPlane(const Ray &ray, float planeY) {
        constexpr float kParallel = 1e-6f;
        if (std::abs(ray.direction.y) < kParallel) {
            return std::nullopt;
        }
        // origin.y + t * direction.y = planeY
        const float t = (planeY - ray.origin.y) / ray.direction.y;
        if (t < 0.0f) {
            return std::nullopt; // the plane is behind the ray
        }
        return XMFLOAT3{ray.origin.x + t * ray.direction.x, planeY, ray.origin.z + t * ray.direction.z};
    }
} // namespace engine::scene
