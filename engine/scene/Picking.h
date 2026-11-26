#ifndef D3D12_ENGINE_PICKING_H
#define D3D12_ENGINE_PICKING_H

// Selects objects with the mouse: a ray from the camera through a pixel, tested against the box of every part.

#include <optional>

#include <DirectXMath.h>

#include "renderdata/Model.h"
#include "scene/Camera.h"
#include "scene/Scene.h"

namespace scene {
    struct Ray {
        DirectX::XMFLOAT3 origin{};
        DirectX::XMFLOAT3 direction{0.0f, 0.0f, 1.0f};
    };

    [[nodiscard]] Ray MakePickRay(const Camera &camera, DirectX::XMFLOAT2 pointPx, DirectX::XMFLOAT2 viewportPx);

    [[nodiscard]] std::optional<float> IntersectRayBox(const Ray &ray, const DirectX::XMFLOAT4X4 &world,
                                                       const render::Aabb &box);

    [[nodiscard]] std::optional<ObjectId> Pick(const Scene &scene, const Ray &ray);

    [[nodiscard]] std::optional<DirectX::XMFLOAT3> IntersectHorizontalPlane(const Ray &ray, float planeY);
} // namespace scene

#endif // D3D12_ENGINE_PICKING_H
