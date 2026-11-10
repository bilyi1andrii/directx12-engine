#include "scene/CameraController.h"

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

namespace engine::scene {
    using namespace DirectX;

    void CameraController::Update(Camera &camera, const core::Input &input, double deltaSeconds, CameraInputs allowed) {
        if (!allowed.mouse || !input.IsKeyDown(VK_RBUTTON)) {
            return;
        }
        m_speedScale =
            std::clamp(m_speedScale * std::pow(kSpeedStep, input.WheelDelta()), kMinSpeedScale, kMaxSpeedScale);
        // Rotation first, so this frame's movement already uses the new direction
        const core::Int2 delta = input.RawMouseDelta();
        AdjustRotation(camera, static_cast<float>(delta.y) * kRotationSpeed,
                       static_cast<float>(delta.x) * kRotationSpeed, 0.0f);
        if (!allowed.keyboard) {
            return;
        }

        const float speed = kMoveSpeed * m_speedScale * static_cast<float>(deltaSeconds * 1000.0);
        XMVECTOR position = XMLoadFloat3(&camera.position);
        const XMVECTOR forward = Forward(camera);
        const XMVECTOR right = Right(camera);

        if (input.IsKeyDown('W')) {
            position = XMVectorAdd(position, XMVectorScale(forward, speed));
        }
        if (input.IsKeyDown('S')) {
            position = XMVectorAdd(position, XMVectorScale(XMVectorNegate(forward), speed));
        }
        if (input.IsKeyDown('A')) {
            position = XMVectorAdd(position, XMVectorScale(XMVectorNegate(right), speed));
        }
        if (input.IsKeyDown('D')) {
            position = XMVectorAdd(position, XMVectorScale(right, speed));
        }
        if (input.IsKeyDown(VK_SPACE)) {
            position = XMVectorAdd(position, XMVectorSet(0.0f, speed, 0.0f, 0.0f));
        }
        if (input.IsKeyDown(VK_LSHIFT)) {
            position = XMVectorAdd(position, XMVectorSet(0.0f, -speed, 0.0f, 0.0f));
        }
        XMStoreFloat3(&camera.position, position);
    }
} // namespace engine::scene
