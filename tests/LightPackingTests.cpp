#include <cmath>
#include <cstdint>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/LightPacking.h"
#include "renderdata/Lights.h"

#include "common/shared_types.hlsli"

using engine::gpu::FrameConstants;
using engine::render::LightSet;
using engine::render::LightType;
using engine::render::LocalLight;
using engine::render::PackLights;
using engine::render::PointLight;
using engine::render::SpotLight;

namespace {
    constexpr float kTolerance = 1e-5f;

    // A point light whose strength tells it apart from the others
    [[nodiscard]] PointLight Point(float strength, bool enabled = true) {
        return {.light = {.type = LightType::Point, .enabled = enabled, .strength = strength}};
    }

    [[nodiscard]] SpotLight Spot(float strength, bool enabled = true) {
        return {.light = {.type = LightType::Spot, .enabled = enabled, .strength = strength}};
    }

    [[nodiscard]] float Length(const DirectX::XMFLOAT3 &v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
} // namespace

TEST(PackLights, CopiesTheEnvironmentAndEachLightsValues) {
    LightSet lights;
    lights.environment.ambientColor = {0.1f, 0.2f, 0.3f};
    lights.environment.ambientStrength = 0.4f;
    lights.environment.specularColor = {0.5f, 0.6f, 0.7f};
    lights.environment.shininess = 32.0f;
    lights.environment.directional.color = {0.8f, 0.9f, 1.0f};
    lights.environment.directional.strength = 2.0f;
    lights.points.push_back(
        {.light = {.color = {1, 0, 0}, .strength = 3.0f, .attenuation = {1, 2, 3}}, .position = {4, 5, 6}});
    lights.spots.push_back({.light = {.type = LightType::Spot, .color = {0, 1, 0}, .strength = 7.0f},
                            .position = {8, 9, 10},
                            .direction = {0, 0, 1}});

    FrameConstants constants{};
    PackLights(lights, constants);

    EXPECT_EQ(constants.ambientColor.y, 0.2f);
    EXPECT_EQ(constants.ambientStrength, 0.4f);
    EXPECT_EQ(constants.specularColor.z, 0.7f);
    EXPECT_EQ(constants.shininess, 32.0f);
    EXPECT_EQ(constants.directionalColor.x, 0.8f);
    EXPECT_EQ(constants.directionalStrength, 2.0f);
    ASSERT_EQ(constants.pointLightCount, 1u);
    EXPECT_EQ(constants.pointLights[0].position.y, 5.0f);
    EXPECT_EQ(constants.pointLights[0].strength, 3.0f);
    EXPECT_EQ(constants.pointLights[0].color.x, 1.0f);
    EXPECT_EQ(constants.pointLights[0].attenuation.z, 3.0f);
    ASSERT_EQ(constants.spotLightCount, 1u);
    EXPECT_EQ(constants.spotLights[0].position.z, 10.0f);
    EXPECT_EQ(constants.spotLights[0].strength, 7.0f);
    EXPECT_EQ(constants.spotLights[0].color.y, 1.0f);
}

// The enabled lights move up: the shader reads the first pointLightCount / spotLightCount elements
TEST(PackLights, SkipsDisabledLights) {
    LightSet lights;
    lights.points = {Point(1.0f, false), Point(2.0f), Point(3.0f, false), Point(4.0f)};
    lights.spots = {Spot(5.0f, false), Spot(6.0f)};

    FrameConstants constants{};
    PackLights(lights, constants);

    ASSERT_EQ(constants.pointLightCount, 2u);
    EXPECT_EQ(constants.pointLights[0].strength, 2.0f);
    EXPECT_EQ(constants.pointLights[1].strength, 4.0f);
    ASSERT_EQ(constants.spotLightCount, 1u);
    EXPECT_EQ(constants.spotLights[0].strength, 6.0f);
}

// A disabled light does not use up one of the places
TEST(PackLights, KeepsTheFirstEightPointAndFourSpotLights) {
    LightSet lights;
    lights.points.push_back(Point(100.0f, false));
    lights.spots.push_back(Spot(100.0f, false));
    for (uint32_t i = 0; i < 12; ++i) {
        lights.points.push_back(Point(static_cast<float>(i)));
        lights.spots.push_back(Spot(static_cast<float>(i)));
    }

    FrameConstants constants{};
    PackLights(lights, constants);

    ASSERT_EQ(constants.pointLightCount, engine::gpu::kMaxPointLights);
    ASSERT_EQ(constants.spotLightCount, engine::gpu::kMaxSpotLights);
    EXPECT_EQ(constants.pointLights[0].strength, 0.0f);
    EXPECT_EQ(constants.pointLights[7].strength, 7.0f);
    EXPECT_EQ(constants.spotLights[3].strength, 3.0f);
}

TEST(PackLights, NormalizesDirections) {
    LightSet lights;
    lights.environment.directional.direction = {0.0f, -3.0f, 4.0f};
    lights.spots.push_back(Spot(1.0f));
    lights.spots.back().direction = {10.0f, 0.0f, 0.0f};
    // No direction at all: straight down, not a division by zero
    lights.spots.push_back(Spot(1.0f));
    lights.spots.back().direction = {0.0f, 0.0f, 0.0f};

    FrameConstants constants{};
    PackLights(lights, constants);

    EXPECT_NEAR(constants.directionalDirection.x, 0.0f, kTolerance);
    EXPECT_NEAR(constants.directionalDirection.y, -0.6f, kTolerance);
    EXPECT_NEAR(constants.directionalDirection.z, 0.8f, kTolerance);
    EXPECT_NEAR(constants.spotLights[0].direction.x, 1.0f, kTolerance);
    EXPECT_NEAR(Length(constants.spotLights[0].direction), 1.0f, kTolerance);
    EXPECT_NEAR(constants.spotLights[1].direction.y, -1.0f, kTolerance);
    EXPECT_NEAR(Length(constants.spotLights[1].direction), 1.0f, kTolerance);
}

// The shader divides by innerCos - outerCos
TEST(PackLights, InnerCosineIsAlwaysGreaterThanOuter) {
    LightSet lights;
    lights.spots = {Spot(1.0f), Spot(1.0f), Spot(1.0f), Spot(1.0f)};
    lights.spots[0].light.innerAngleDegrees = 18.0f; // the usual case
    lights.spots[0].light.outerAngleDegrees = 28.0f;
    lights.spots[1].light.innerAngleDegrees = 40.0f; // inner wider than outer
    lights.spots[1].light.outerAngleDegrees = 20.0f;
    lights.spots[2].light.innerAngleDegrees = 30.0f; // equal
    lights.spots[2].light.outerAngleDegrees = 30.0f;
    lights.spots[3].light.innerAngleDegrees = 0.0f; // no cone at all
    lights.spots[3].light.outerAngleDegrees = 0.0f;

    FrameConstants constants{};
    PackLights(lights, constants);

    ASSERT_EQ(constants.spotLightCount, 4u);
    EXPECT_NEAR(constants.spotLights[0].innerCos, std::cos(DirectX::XMConvertToRadians(18.0f)), kTolerance);
    EXPECT_NEAR(constants.spotLights[0].outerCos, std::cos(DirectX::XMConvertToRadians(28.0f)), kTolerance);
    EXPECT_NEAR(constants.spotLights[1].outerCos, std::cos(DirectX::XMConvertToRadians(20.0f)), kTolerance);
    for (uint32_t i = 0; i < constants.spotLightCount; ++i) {
        EXPECT_GT(constants.spotLights[i].innerCos, constants.spotLights[i].outerCos) << "spot " << i;
    }
}

// Its colour and direction stay, so only the strength can switch it off
TEST(PackLights, DisabledDirectionalLightHasStrengthZero) {
    LightSet lights;
    lights.environment.directional.enabled = false;
    lights.environment.directional.strength = 0.6f;

    FrameConstants constants{};
    constants.directionalStrength = 5.0f; // left over from somewhere else
    PackLights(lights, constants);

    EXPECT_EQ(constants.directionalStrength, 0.0f);
}
