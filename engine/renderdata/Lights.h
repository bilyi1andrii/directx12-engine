#ifndef D3D12_ENGINE_LIGHTS_H
#define D3D12_ENGINE_LIGHTS_H

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Model.h"

namespace render {
    // One light without a position, like the sun
    struct DirectionalLight {
        bool enabled = true;
        DirectX::XMFLOAT3 direction{-0.5f, -1.0f, 1.0f}; // where it shines, world space; any length
        DirectX::XMFLOAT3 color{1.0f, 0.97f, 0.90f};
        float strength = 0.6f;
    };

    // Lighting that belongs to the whole scene, not to a place in it. Specular colour and shininess are the same
    // for every material until materials have their own GPU data.
    struct LightEnvironment {
        DirectX::XMFLOAT3 ambientColor{1.0f, 1.0f, 1.0f};
        float ambientStrength = 0.16f;
        DirectX::XMFLOAT3 specularColor{1.0f, 1.0f, 1.0f};
        float shininess = 128.0f; // higher = smaller, sharper highlights
        DirectionalLight directional;
    };

    enum class LightType : uint8_t { Point, Spot };

    // The settings of a point or spot light. Its position and direction are not stored here.
    // In a scene they come from the light's object; in a LightSet, from PointLight and SpotLight.
    struct LocalLight {
        LightType type = LightType::Point;
        bool enabled = true;
        DirectX::XMFLOAT3 color{1.0f, 1.0f, 1.0f};
        float strength = 1.0f;
        // The light fades with distance d as 1 / (x + y * d + z * d * d)
        DirectX::XMFLOAT3 attenuation{1.0f, 0.045f, 0.0075f};
        // Spot only: half angles of the cone. Full strength inside the inner one, nothing outside the outer one.
        float innerAngleDegrees = 18.0f;
        float outerAngleDegrees = 28.0f;
    };

    struct PointLight {
        LocalLight light;
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f}; // world space
    };

    struct SpotLight {
        LocalLight light;
        DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};  // world space
        DirectX::XMFLOAT3 direction{0.0f, 0.0f, 1.0f}; // where it points, world space; any length
    };

    // All lights of one frame, in world space, as Renderer::Render takes them.
    // The renderer uses the first gpu::kMaxPointLights and gpu::kMaxSpotLights enabled lights and ignores the rest.
    struct LightSet {
        LightEnvironment environment;
        std::vector<PointLight> points;
        std::vector<SpotLight> spots;
        Aabb shadowBounds;
    };
} // namespace render

#endif // D3D12_ENGINE_LIGHTS_H
