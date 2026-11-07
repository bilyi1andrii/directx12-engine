#ifndef D3D12_ENGINE_CAMERAPATH_H
#define D3D12_ENGINE_CAMERAPATH_H

#include <cstdint>
#include <span>

#include <DirectXMath.h>

namespace engine::scene {
    // One point of a scripted camera path, used by the bench: at `frame` the camera is at `position` and looks at
    // `target`. Frame numbers instead of time, so frame N shows the same view on every machine.
    struct CameraKeyframe {
        uint32_t frame = 0;
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 target{0.0f, 0.0f, 1.0f};
    };

    // Apply with `camera.position = pose.position` and `LookAt(camera, pose.target)`
    struct CameraPose {
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 target{0.0f, 0.0f, 1.0f};
    };

    // Interpolates linearly between the keyframes before and after `frame`, and stays at the first or last pose
    // outside the path. Needs at least one keyframe, with increasing frames.
    [[nodiscard]] CameraPose EvaluateCameraPath(std::span<const CameraKeyframe> keyframes, uint32_t frame);
} // namespace engine::scene

#endif // D3D12_ENGINE_CAMERAPATH_H
