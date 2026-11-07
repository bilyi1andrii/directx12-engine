#ifndef D3D12_ENGINE_CAMERACONTROLLER_H
#define D3D12_ENGINE_CAMERACONTROLLER_H

#include "core/Input.h"
#include "scene/Camera.h"

namespace engine::scene {
    // Which input the camera controller may use this frame. The app withholds what the debug UI is using
    // (mouse over an ImGui window, keys typed into a widget).
    struct CameraInputs {
        bool mouse = true;    // looking around
        bool keyboard = true; // moving
    };

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

        // Applies this frame's input, only the kinds `allowed` lets through. deltaSeconds: time since the previous
        // frame.
        void Update(Camera &camera, const core::Input &input, double deltaSeconds, CameraInputs allowed = {}) const;
    };
} // namespace engine::scene

#endif // D3D12_ENGINE_CAMERACONTROLLER_H
