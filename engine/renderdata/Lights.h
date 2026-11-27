#ifndef D3D12_ENGINE_LIGHTS_H
#define D3D12_ENGINE_LIGHTS_H

// The lights of a frame: the sun-like directional light, the ambient and specular settings, 
// and the point and spot lights placed in the scene.

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Model.h"

namespace render {
    struct DirectionalLight {
        bool enabled = true;
        DirectX::XMFLOAT3 direction{-0.5f, -1.0f, 1.0f}; // world space, any length
        DirectX::XMFLOAT3 color{1.0f, 0.97f, 0.90f};
        float strength = 0.6f;
    };

    struct LightEnvironment {
        DirectX::XMFLOAT3 ambientColor{1.0f, 1.0f, 1.0f};
        float ambientStrength = 0.16f;
        DirectX::XMFLOAT3 specularColor{1.0f, 1.0f, 1.0f};
        float shininess = 128.0f;
        DirectionalLight directional;
    };

    enum class LightType : uint8_t { Point, Spot };

    // light component, position and direction come from object
    struct LocalLight {
        LightType type = LightType::Point;
        bool enabled = true;
        DirectX::XMFLOAT3 color{1.0f, 1.0f, 1.0f};
        float strength = 1.0f;
        DirectX::XMFLOAT3 attenuation{1.0f, 0.045f, 0.0075f}; // constant, linear, quadratic
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
        DirectX::XMFLOAT3 direction{0.0f, 0.0f, 1.0f}; // world space, any length
    };

    struct LightSet {
        LightEnvironment environment;
        std::vector<PointLight> points;
        std::vector<SpotLight> spots;
        Aabb shadowBounds; // all parts' world boxes, shadow map fits it
    };
} // namespace render

#endif // D3D12_ENGINE_LIGHTS_H
