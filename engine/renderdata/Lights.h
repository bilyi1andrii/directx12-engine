#ifndef D3D12_ENGINE_LIGHTS_H
#define D3D12_ENGINE_LIGHTS_H

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

namespace engine::render {
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

    // A point or spot light without its place: position and direction come from where it stands in the world
    // (in a scene: the light object's world matrix)
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

    // Every light of a frame, in world space: what Renderer::Render reads. The renderer uses the first
    // gpu::kMaxPointLights and gpu::kMaxSpotLights enabled lights and ignores the rest.
    struct LightSet {
        LightEnvironment environment;
        std::vector<PointLight> points;
        std::vector<SpotLight> spots;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_LIGHTS_H
