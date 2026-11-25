#include <Windows.h>
#include <gtest/gtest.h>

#include "core/Input.h"
#include "core/Window.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"

using core::Input;
using core::WindowMessage;
using scene::Camera;
using scene::CameraController;
using scene::CameraInputs;

namespace {
    constexpr double kFrameSeconds = 0.016;
    // How far W moves the camera in one frame
    constexpr float kStep = CameraController::kMoveSpeed * static_cast<float>(kFrameSeconds * 1000.0);

    void Send(Input &input, UINT message, WPARAM wParam = 0) {
        input.HandleMessage(WindowMessage{.hwnd = nullptr, .message = message, .wParam = wParam, .lParam = 0});
    }

    // Turns the mouse wheel by `notches` (positive: away from the user)
    void Wheel(Input &input, int notches) {
        Send(input, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(static_cast<short>(notches * WHEEL_DELTA))));
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

// Without the right button the wheel belongs to the rest of the app; with it, each notch is one speed step
TEST(CameraController, WheelChangesTheSpeedOnlyWithTheRightMouseButton) {
    CameraController controller;
    Camera camera;
    Input input;
    Wheel(input, 2);
    controller.Update(camera, input, kFrameSeconds);
    EXPECT_FLOAT_EQ(controller.SpeedScale(), 1.0f);

    Send(input, WM_RBUTTONDOWN);
    controller.Update(camera, input, kFrameSeconds);
    EXPECT_FLOAT_EQ(controller.SpeedScale(), CameraController::kSpeedStep * CameraController::kSpeedStep);
    input.EndFrame();

    Wheel(input, -1);
    controller.Update(camera, input, kFrameSeconds);
    EXPECT_FLOAT_EQ(controller.SpeedScale(), CameraController::kSpeedStep);
    Send(input, WM_RBUTTONUP);
}

TEST(CameraController, SpeedScaleMultipliesTheMovement) {
    CameraController controller;
    Camera camera;
    Input input;
    Send(input, WM_RBUTTONDOWN);
    Send(input, WM_KEYDOWN, 'W');
    Wheel(input, 1);
    controller.Update(camera, input, kFrameSeconds);
    Send(input, WM_RBUTTONUP);
    EXPECT_NEAR(camera.position.z, kStep * CameraController::kSpeedStep, 1e-6f);
}

TEST(CameraController, SpeedScaleStaysWithinItsLimits) {
    CameraController controller;
    Camera camera;
    Input input;
    Send(input, WM_RBUTTONDOWN);
    Wheel(input, 100);
    controller.Update(camera, input, kFrameSeconds);
    EXPECT_FLOAT_EQ(controller.SpeedScale(), CameraController::kMaxSpeedScale);
    input.EndFrame();

    Wheel(input, -100);
    controller.Update(camera, input, kFrameSeconds);
    EXPECT_FLOAT_EQ(controller.SpeedScale(), CameraController::kMinSpeedScale);
    Send(input, WM_RBUTTONUP);
}
