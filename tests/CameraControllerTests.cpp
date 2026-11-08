#include <Windows.h>
#include <gtest/gtest.h>

#include "core/Input.h"
#include "core/Window.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"

using engine::core::Input;
using engine::core::WindowMessage;
using engine::scene::Camera;
using engine::scene::CameraController;
using engine::scene::CameraInputs;

namespace {
    constexpr double kFrameSeconds = 0.016;
    // How far W moves the camera in one frame
    constexpr float kStep = CameraController::kMoveSpeed * static_cast<float>(kFrameSeconds * 1000.0);

    void Send(Input &input, UINT message, WPARAM wParam = 0) {
        input.HandleMessage(WindowMessage{.hwnd = nullptr, .message = message, .wParam = wParam, .lParam = 0});
    }

    // A camera at the origin looking along +Z after one frame with W held, and the right mouse button if asked
    [[nodiscard]] Camera AfterOneFrameOfW(bool rightButton, CameraInputs allowed = {}) {
        Input input;
        Send(input, WM_KEYDOWN, 'W');
        if (rightButton) {
            Send(input, WM_RBUTTONDOWN);
        }
        Camera camera;
        CameraController{}.Update(camera, input, kFrameSeconds, allowed);
        if (rightButton) {
            Send(input, WM_RBUTTONUP); // releases the mouse capture taken on the button-down
        }
        return camera;
    }
} // namespace

// The keys fly the camera only while the right mouse button is held: without it W is the gizmo's "move" shortcut
TEST(CameraController, KeysMoveTheCameraOnlyWithTheRightMouseButton) {
    const Camera still = AfterOneFrameOfW(false);
    EXPECT_FLOAT_EQ(still.position.z, 0.0f);

    const Camera moved = AfterOneFrameOfW(true);
    EXPECT_NEAR(moved.position.x, 0.0f, 1e-6f);
    EXPECT_NEAR(moved.position.z, kStep, 1e-6f);
}

// Over an ImGui window (mouse withheld) the right button does not count; a widget taking typed keys withholds them
TEST(CameraController, WithheldInputDoesNotMoveTheCamera) {
    EXPECT_FLOAT_EQ(AfterOneFrameOfW(true, {.mouse = false, .keyboard = true}).position.z, 0.0f);
    EXPECT_FLOAT_EQ(AfterOneFrameOfW(true, {.mouse = true, .keyboard = false}).position.z, 0.0f);
}
