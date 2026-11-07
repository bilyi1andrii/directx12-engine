#ifndef D3D12_ENGINE_CAMERAPATH_H
#define D3D12_ENGINE_CAMERAPATH_H

#include <cstdint>
#include <span>

#include <DirectXMath.h>

namespace engine::scene {
    // A point of a scripted camera path (benchmarks): at frame `frame` the camera is at `position` and looks at
    // `target`. Paths are indexed by frame number, not time, so frame N shows the same view on every machine
    // and in both engines (DX11 copies the keyframes).
    struct CameraKeyframe {
        uint32_t frame = 0;
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 target{0.0f, 0.0f, 1.0f};
    };

    // Where the camera is and what it looks at; apply with `camera.position = pose.position` and
    // `LookAt(camera, pose.target)`
    struct CameraPose {
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 target{0.0f, 0.0f, 1.0f};
    };

    // The pose at `frame`: position and target interpolated linearly between the two keyframes around it.
    // Before the first keyframe the first pose, after the last one the last pose.
    // `keyframes`: at least one, frames strictly increasing (asserted).
    [[nodiscard]] CameraPose EvaluateCameraPath(std::span<const CameraKeyframe> keyframes, uint32_t frame);
} // namespace engine::scene

#endif // D3D12_ENGINE_CAMERAPATH_H
