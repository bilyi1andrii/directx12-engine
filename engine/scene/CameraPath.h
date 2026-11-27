#ifndef D3D12_ENGINE_CAMERAPATH_H
#define D3D12_ENGINE_CAMERAPATH_H

// A scripted camera path for the bench: keyframes by frame number, so frame N shows the same view on every machine.

#include <cstdint>
#include <span>

#include <DirectXMath.h>

namespace scene {
    struct CameraKeyframe {
        uint32_t frame = 0; // frame number
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 target{0.0f, 0.0f, 1.0f};
    };

    struct CameraPose {
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 target{0.0f, 0.0f, 1.0f};
    };

    [[nodiscard]] CameraPose EvaluateCameraPath(std::span<const CameraKeyframe> keyframes, uint32_t frame);
} // namespace scene

#endif // D3D12_ENGINE_CAMERAPATH_H
