#ifndef D3D12_ENGINE_CAMERACONTROLLER_H
#define D3D12_ENGINE_CAMERACONTROLLER_H

#include "core/Input.h"
#include "scene/Camera.h"

namespace engine::scene {
    // Fly camera controls:
    //   right mouse button + mouse   look around
    //   W / S                        forward / back (along the view direction, pitch included)
    //   A / D                        left / right
    //   Space / Left Shift           up / down (world Y)
    class CameraController {
    public:
        // Radians per raw mouse count
        static constexpr float kRotationSpeed = 0.01f;
        // World units per millisecond
        static constexpr float kMoveSpeed = 0.0125f;

        // Applies this frame's input. deltaSeconds: time since the previous frame.
        void Update(Camera &camera, const core::Input &input, double deltaSeconds) const;
    };
} // namespace engine::scene

#endif // D3D12_ENGINE_CAMERACONTROLLER_H
