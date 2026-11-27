#ifndef D3D12_ENGINE_CAMERA_H
#define D3D12_ENGINE_CAMERA_H

// The camera as plain data, and the view and projection it gives the renderer (reversed depth: near is 1, far is 0).
// Also the directions the camera moves in, and turning it towards a point.

#include <DirectXMath.h>

#include "renderdata/FrameView.h"

namespace scene {
    struct Camera {
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 rotation{0.0f, 0.0f, 0.0f}; // pitch, yaw, roll in radians
        float fovDegrees = 90.0f;
        float nearZ = 0.1f;
        float farZ = 1000.0f;
    };

    [[nodiscard]] DirectX::XMMATRIX ViewMatrix(const Camera &camera);
    [[nodiscard]] DirectX::XMMATRIX ProjectionMatrix(const Camera &camera, float aspectRatio);

    [[nodiscard]] render::FrameView MakeFrameView(const Camera &camera, float aspectRatio);

    [[nodiscard]] DirectX::XMVECTOR Forward(const Camera &camera);
    [[nodiscard]] DirectX::XMVECTOR Right(const Camera &camera);

    void AdjustRotation(Camera &camera, float pitch, float yaw, float roll);

    void LookAt(Camera &camera, const DirectX::XMFLOAT3 &target);
} // namespace scene

#endif // D3D12_ENGINE_CAMERA_H
