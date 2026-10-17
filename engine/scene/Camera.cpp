#include "scene/Camera.h"

#include <cmath>

namespace engine::scene {
    using namespace DirectX;

    namespace {
        // DX11 MotionObj.h DEFAULT_FORWARD_VECTOR / DEFAULT_UP_VECTOR / DEFAULT_RIGHT_VECTOR
        const XMVECTOR kDefaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR kDefaultUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMVECTOR kDefaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    } // namespace

    XMMATRIX ViewMatrix(const Camera &camera) {
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(camera.rotation.x, camera.rotation.y, camera.rotation.z);
        const XMVECTOR position = XMLoadFloat3(&camera.position);
        const XMVECTOR target = XMVectorAdd(XMVector3TransformCoord(kDefaultForward, rotation), position);
        const XMVECTOR up = XMVector3TransformCoord(kDefaultUp, rotation);
        return XMMatrixLookAtLH(position, target, up);
    }

    XMMATRIX ProjectionMatrix(const Camera &camera, float aspectRatio) {
        const float fovRadians = (camera.fovDegrees / 360.0f) * XM_2PI;
        return XMMatrixPerspectiveFovLH(fovRadians, aspectRatio, camera.nearZ, camera.farZ);
    }

    XMVECTOR Forward(const Camera &camera) {
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(camera.rotation.x, camera.rotation.y, 0.0f);
        return XMVector3TransformCoord(kDefaultForward, rotation);
    }

    XMVECTOR Right(const Camera &camera) {
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(camera.rotation.x, camera.rotation.y, 0.0f);
        return XMVector3TransformCoord(kDefaultRight, rotation);
    }

    void AdjustRotation(Camera &camera, float pitch, float yaw, float roll) {
        camera.rotation.x += pitch;
        camera.rotation.y += yaw;
        camera.rotation.z += roll;

        constexpr float kEps = 0.001f;
        constexpr float kMaxPitch = XM_PIDIV2 - kEps;
        camera.rotation.x = camera.rotation.x > kMaxPitch ? kMaxPitch : camera.rotation.x;
        camera.rotation.x = camera.rotation.x < -kMaxPitch ? -kMaxPitch : camera.rotation.x;
    }

    void LookAt(Camera &camera, const XMFLOAT3 &target) {
        const XMFLOAT3 &pos = camera.position;
        if (target.x == pos.x && target.y == pos.y && target.z == pos.z) {
            return;
        }

        // From the target to the camera
        const float dx = pos.x - target.x;
        const float dy = pos.y - target.y;
        const float dz = pos.z - target.z;

        float pitch = 0.0f;
        if (dy != 0.0f) {
            const float distance = std::sqrt(dx * dx + dz * dz);
            pitch = std::atan(dy / distance);
        }

        float yaw = 0.0f;
        if (dx != 0.0f) {
            yaw = std::atan(dx / dz);
        }
        if (dz > 0.0f) {
            yaw += XM_PI;
        }

        camera.rotation = {pitch, yaw, 0.0f};
    }
} // namespace engine::scene
