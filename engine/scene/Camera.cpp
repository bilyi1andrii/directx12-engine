#include "scene/Camera.h"

#include <cmath>

namespace engine::scene {
    using namespace DirectX;

    namespace {
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

    render::FrameView MakeFrameView(const Camera &camera, float aspectRatio) {
        render::FrameView view{};
        XMStoreFloat4x4(&view.view, ViewMatrix(camera));
        XMStoreFloat4x4(&view.projection, ProjectionMatrix(camera, aspectRatio));
        view.cameraPosition = camera.position;
        return view;
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

        // Direction from the camera to the target. atan2 works in every direction; DX11's atan(x / z) looked the
        // wrong way when z == 0.
        const float dx = target.x - pos.x;
        const float dy = target.y - pos.y;
        const float dz = target.z - pos.z;
        const float pitch = std::atan2(-dy, std::sqrt(dx * dx + dz * dz)); // positive looks down
        const float yaw = std::atan2(dx, dz);

        camera.rotation = {pitch, yaw, 0.0f};
    }
} // namespace engine::scene
