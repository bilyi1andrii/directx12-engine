#include "renderdata/LightPacking.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <DirectXMath.h>

#include "common/shared_types.hlsli"

namespace engine::render {
    namespace {
        // `direction` at unit length; straight down if it has no length
        [[nodiscard]] DirectX::XMFLOAT3 UnitDirection(const DirectX::XMFLOAT3 &direction) {
            using namespace DirectX;

            const XMVECTOR vector = XMLoadFloat3(&direction);
            XMFLOAT3 result{0.0f, -1.0f, 0.0f};
            if (XMVectorGetX(XMVector3LengthSq(vector)) > 1e-12f) {
                XMStoreFloat3(&result, XMVector3Normalize(vector));
            }
            return result;
        }
    } // namespace

    void PackLights(const LightSet &lights, gpu::FrameConstants &constants) {
        const LightEnvironment &environment = lights.environment;
        constants.ambientColor = environment.ambientColor;
        constants.ambientStrength = environment.ambientStrength;
        constants.specularColor = environment.specularColor;
        // The shader computes pow(x, shininess), where x can be 0, and pow(0, 0) gives NaN on the GPU
        constants.shininess = std::max(environment.shininess, 1.0f);

        const DirectionalLight &directional = environment.directional;
        constants.directionalDirection = UnitDirection(directional.direction);
        constants.directionalColor = directional.color;
        // The shader has no on/off flag: a light of strength 0 adds nothing
        constants.directionalStrength = directional.enabled ? directional.strength : 0.0f;

        uint32_t pointCount = 0;
        for (const PointLight &point : lights.points) {
            if (!point.light.enabled) {
                continue;
            }
            if (pointCount == gpu::kMaxPointLights) {
                break;
            }
            gpu::PointLightData &data = constants.pointLights[pointCount++];
            data.position = point.position;
            data.strength = point.light.strength;
            data.color = point.light.color;
            data.attenuation = point.light.attenuation;
        }
        constants.pointLightCount = pointCount;

        uint32_t spotCount = 0;
        for (const SpotLight &spot : lights.spots) {
            if (!spot.light.enabled) {
                continue;
            }
            if (spotCount == gpu::kMaxSpotLights) {
                break;
            }
            // The shader divides by innerCos - outerCos. So the inner cone stays inside the outer one, and equal
            // angles get a small gap.
            constexpr float kMinCosGap = 1e-4f;
            const float outerDegrees = std::clamp(spot.light.outerAngleDegrees, 0.0f, 90.0f);
            const float innerDegrees = std::clamp(spot.light.innerAngleDegrees, 0.0f, outerDegrees);
            const float innerCos = std::cos(DirectX::XMConvertToRadians(innerDegrees));
            // The gap widens the outer cone, not the inner one: innerCos above 1 would dim the spot's centre
            const float outerCos = std::min(std::cos(DirectX::XMConvertToRadians(outerDegrees)), innerCos - kMinCosGap);

            gpu::SpotLightData &data = constants.spotLights[spotCount++];
            data.position = spot.position;
            data.strength = spot.light.strength;
            data.direction = UnitDirection(spot.direction);
            data.innerCos = innerCos;
            data.color = spot.light.color;
            data.outerCos = outerCos;
            data.attenuation = spot.light.attenuation;
        }
        constants.spotLightCount = spotCount;
    }

    PackedLightCounts CountPackedLights(const LightSet &lights) {
        // Packing for real keeps the counts in step with PackLights' rules
        gpu::FrameConstants constants{};
        PackLights(lights, constants);
        return {.directional = lights.environment.directional.enabled ? 1u : 0u,
                .points = constants.pointLightCount,
                .spots = constants.spotLightCount};
    }
} // namespace engine::render
