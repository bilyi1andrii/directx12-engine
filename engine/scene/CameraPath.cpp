#include "CameraPath.h"

#include <algorithm>
#include <functional>

#include "core/Assert.h"

namespace scene {
    namespace {
        [[nodiscard]] DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3 &a, const DirectX::XMFLOAT3 &b, float t) {
            DirectX::XMFLOAT3 result{};
            DirectX::XMStoreFloat3(&result,
                                   DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&a), DirectX::XMLoadFloat3(&b), t));
            return result;
        }
    } // namespace

    CameraPose EvaluateCameraPath(std::span<const CameraKeyframe> keyframes, uint32_t frame) {
        ENGINE_ASSERT_MSG(!keyframes.empty(), "a camera path needs at least one keyframe");
        ENGINE_ASSERT_MSG(std::ranges::adjacent_find(keyframes, std::greater_equal{}, &CameraKeyframe::frame) ==
                              keyframes.end(),
                          "camera keyframes must have strictly increasing frames");

        // The first keyframe after `frame`
        const auto next = std::ranges::upper_bound(keyframes, frame, {}, &CameraKeyframe::frame);
        if (next == keyframes.begin()) {
            return {.position = keyframes.front().position, .target = keyframes.front().target};
        }
        if (next == keyframes.end()) {
            return {.position = keyframes.back().position, .target = keyframes.back().target};
        }
        const CameraKeyframe &from = *(next - 1);
        const CameraKeyframe &to = *next;
        const float t = static_cast<float>(frame - from.frame) / static_cast<float>(to.frame - from.frame);
        return {.position = Lerp(from.position, to.position, t), .target = Lerp(from.target, to.target, t)};
    }
} // namespace scene
