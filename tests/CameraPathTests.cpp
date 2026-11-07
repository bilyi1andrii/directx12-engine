#include <array>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "scene/CameraPath.h"

using engine::scene::CameraKeyframe;
using engine::scene::CameraPose;
using engine::scene::EvaluateCameraPath;

namespace {
    void ExpectFloat3(const DirectX::XMFLOAT3 &actual, float x, float y, float z) {
        EXPECT_FLOAT_EQ(actual.x, x);
        EXPECT_FLOAT_EQ(actual.y, y);
        EXPECT_FLOAT_EQ(actual.z, z);
    }

    // Three keyframes with uneven gaps (10 and 30 frames), so a wrong segment or a wrong t shows up
    constexpr std::array kPath{
        CameraKeyframe{.frame = 10, .position = {0.0f, 0.0f, 0.0f}, .target = {0.0f, 0.0f, 1.0f}},
        CameraKeyframe{.frame = 20, .position = {10.0f, 0.0f, 0.0f}, .target = {10.0f, 0.0f, 1.0f}},
        CameraKeyframe{.frame = 50, .position = {10.0f, 30.0f, 0.0f}, .target = {10.0f, 0.0f, 31.0f}},
    };
} // namespace

TEST(CameraPath, KeyframeFramesGiveTheirPoses) {
    for (const CameraKeyframe &keyframe : kPath) {
        const CameraPose pose = EvaluateCameraPath(kPath, keyframe.frame);
        ExpectFloat3(pose.position, keyframe.position.x, keyframe.position.y, keyframe.position.z);
        ExpectFloat3(pose.target, keyframe.target.x, keyframe.target.y, keyframe.target.z);
    }
}

TEST(CameraPath, InterpolatesLinearlyBetweenKeyframes) {
    // Halfway through the first segment
    const CameraPose first = EvaluateCameraPath(kPath, 15);
    ExpectFloat3(first.position, 5.0f, 0.0f, 0.0f);
    ExpectFloat3(first.target, 5.0f, 0.0f, 1.0f);
    // A third of the way through the second segment (frame 30 of 20..50)
    const CameraPose second = EvaluateCameraPath(kPath, 30);
    ExpectFloat3(second.position, 10.0f, 10.0f, 0.0f);
    ExpectFloat3(second.target, 10.0f, 0.0f, 11.0f);
}

TEST(CameraPath, ClampsOutsideThePath) {
    ExpectFloat3(EvaluateCameraPath(kPath, 0).position, 0.0f, 0.0f, 0.0f);
    ExpectFloat3(EvaluateCameraPath(kPath, 1000).position, 10.0f, 30.0f, 0.0f);
    // One keyframe: the same pose at every frame
    const std::array single{kPath[1]};
    ExpectFloat3(EvaluateCameraPath(single, 0).position, 10.0f, 0.0f, 0.0f);
    ExpectFloat3(EvaluateCameraPath(single, 99).target, 10.0f, 0.0f, 1.0f);
}
