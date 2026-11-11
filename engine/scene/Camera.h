#ifndef D3D12_ENGINE_CAMERA_H
#define D3D12_ENGINE_CAMERA_H

#include <DirectXMath.h>

#include "renderdata/FrameView.h"

namespace engine::scene {
    // A perspective camera as plain data: left-handed, +Z forward, +Y up, rotation applied as
    // XMMatrixRotationRollPitchYaw(pitch, yaw, roll).
    struct Camera {
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        // Radians: x = pitch (positive looks down), y = yaw (positive turns right), z = roll
        DirectX::XMFLOAT3 rotation{0.0f, 0.0f, 0.0f};
        float fovDegrees = 90.0f; // vertical
        float nearZ = 0.1f;
        float farZ = 1000.0f;
    };

    // World -> view
    [[nodiscard]] DirectX::XMMATRIX ViewMatrix(const Camera &camera);
    // View -> clip, reversed depth (1 near, 0 far)
    [[nodiscard]] DirectX::XMMATRIX ProjectionMatrix(const Camera &camera, float aspectRatio);

    // What the renderer needs from the camera
    [[nodiscard]] render::FrameView MakeFrameView(const Camera &camera, float aspectRatio);

    // Movement directions from pitch and yaw, ignoring roll.
    // Forward follows the pitch, so moving forward while looking down also moves down.
    [[nodiscard]] DirectX::XMVECTOR Forward(const Camera &camera);
    [[nodiscard]] DirectX::XMVECTOR Right(const Camera &camera);

    // Adds to the rotation and keeps the pitch just short of straight up/down, where the view
    // matrix would flip.
    void AdjustRotation(Camera &camera, float pitch, float yaw, float roll);

    // Turns the camera to look at `target`; roll becomes 0.
    // Does nothing if `target` is the camera position.
    void LookAt(Camera &camera, const DirectX::XMFLOAT3 &target);
} // namespace engine::scene

#endif // D3D12_ENGINE_CAMERA_H
