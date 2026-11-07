#ifndef D3D12_ENGINE_PICKING_H
#define D3D12_ENGINE_PICKING_H

#include <optional>

#include <DirectXMath.h>

#include "scene/Camera.h"

namespace engine::scene {
    // A half-line in world space: every origin + t * direction with t >= 0
    struct Ray {
        DirectX::XMFLOAT3 origin;
        DirectX::XMFLOAT3 direction; // unit length
    };

    // The ray from the camera through the point (x, y) of a view `width` x `height` pixels large, (0, 0) at its
    // top-left corner: what is under the mouse at (x, y). width and height must be > 0.
    [[nodiscard]] Ray ScreenRay(const Camera &camera, float x, float y, float width, float height);

    // Where `ray` meets the horizontal plane y = `planeY`.
    [[nodiscard]] std::optional<DirectX::XMFLOAT3> IntersectHorizontalPlane(const Ray &ray, float planeY);
} // namespace engine::scene

#endif // D3D12_ENGINE_PICKING_H
