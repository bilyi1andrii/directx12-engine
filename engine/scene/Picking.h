#ifndef D3D12_ENGINE_PICKING_H
#define D3D12_ENGINE_PICKING_H

#include <optional>

#include <DirectXMath.h>

#include "renderdata/Model.h"
#include "scene/Camera.h"
#include "scene/Scene.h"

// Selecting objects with the mouse: a ray from the camera through a pixel, tested against the objects' boxes.
// CPU only (no picking pass on the GPU).
namespace scene {
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

    // How far along `ray` it enters `box` placed by `world`: 0 if it starts inside, nullopt if it misses or `world`
    // has scale 0. `world` may rotate and scale the box, and the box may be flat.
    [[nodiscard]] std::optional<float> IntersectRayBox(const Ray &ray, const DirectX::XMFLOAT4X4 &world,
                                                       const render::Aabb &box);

    // The object `ray` selects: the one whose part's box it enters first, static objects included. A box the ray
    // starts inside is ignored, so a click on empty space selects nothing. Checks every part: call it on clicks only.
    [[nodiscard]] std::optional<ObjectId> Pick(const Scene &scene, const Ray &ray);

    // Where `ray` meets the horizontal plane y = `planeY`.
    [[nodiscard]] std::optional<DirectX::XMFLOAT3> IntersectHorizontalPlane(const Ray &ray, float planeY);
} // namespace scene

#endif // D3D12_ENGINE_PICKING_H
