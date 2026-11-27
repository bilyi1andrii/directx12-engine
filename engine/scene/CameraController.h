#ifndef D3D12_ENGINE_CAMERACONTROLLER_H
#define D3D12_ENGINE_CAMERACONTROLLER_H

// Flies the camera with the mouse and keys while the right mouse button is held; the wheel changes the speed.

#include "core/Input.h"
#include "scene/Camera.h"

namespace scene {
    struct CameraInputs {
        bool mouse = true;
        bool keyboard = true;
    };

    class CameraController {
    public:
        static constexpr float kRotationSpeed = 0.01f;
        static constexpr float kMoveSpeed = 0.0125f; // units per millisecond
        static constexpr float kSpeedStep = 1.25f;
        static constexpr float kMinSpeedScale = 1.0f / 8.0f;
        static constexpr float kMaxSpeedScale = 8.0f;

        void Update(Camera &camera, const core::Input &input, double deltaSeconds, CameraInputs allowed = {});

        [[nodiscard]] float SpeedScale() const noexcept { return m_speedScale; }

    private:
        float m_speedScale = 1.0f;
    };
} // namespace scene

#endif // D3D12_ENGINE_CAMERACONTROLLER_H
