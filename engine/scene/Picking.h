#ifndef D3D12_ENGINE_PICKING_H
#define D3D12_ENGINE_PICKING_H

#include <optional>

#include <DirectXMath.h>

#include "scene/Camera.h"

// Selecting objects with the mouse: a ray from the camera through a pixel, tested against the objects' boxes.
// CPU only (no picking pass on the GPU).
namespace engine::scene {
    // A half-line in world space: the points origin + t * direction for t >= 0. `direction` has length 1, so t is
    // a distance in world units.
    struct Ray {
        DirectX::XMFLOAT3 origin{};
        DirectX::XMFLOAT3 direction{0.0f, 0.0f, 1.0f};
    };

    // The ray from `camera` through a point of the viewport, starting on the near plane. `pointPx`: client-area
    // pixels from the top-left corner, as floats (the centre of pixel (x, y) is (x + 0.5, y + 0.5)).
    // `viewportPx`: the client size, which also gives the aspect ratio (as for the renderer's projection).
    [[nodiscard]] Ray MakePickRay(const Camera &camera, DirectX::XMFLOAT2 pointPx, DirectX::XMFLOAT2 viewportPx);

    // Where `ray` first enters the box [-1, 1]^3 (assets::MakeCube) placed by `world`: the distance along the ray,
    // 0 if the ray starts inside it, nullopt if it misses (or `world` cannot be inverted, e.g. scale 0).
    // Any affine `world` works, rotated and scaled boxes included.
    [[nodiscard]] std::optional<float> IntersectRayBox(const Ray &ray, const DirectX::XMFLOAT4X4 &world);

    // Where `ray` meets the horizontal plane y = `planeY`.
    [[nodiscard]] std::optional<DirectX::XMFLOAT3> IntersectHorizontalPlane(const Ray &ray, float planeY);
} // namespace engine::scene

#endif // D3D12_ENGINE_PICKING_H
