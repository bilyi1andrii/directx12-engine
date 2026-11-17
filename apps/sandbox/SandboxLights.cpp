#include "SandboxLights.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
#include "renderdata/Material.h"

namespace sandbox {
    namespace {
        using DirectX::XMFLOAT3;
        using DirectX::XMFLOAT4X4;
        using engine::render::LightType;
        using engine::render::LocalLight;

        // Where both spot lights point: down and away from the camera's starting place
        constexpr XMFLOAT3 kSpotDirection{-0.3f, -1.0f, 1.2f};

        // Radius of the sphere that shows where a light is
        constexpr float kMarkerRadius = 0.25f;

        // The world matrix of a light's sphere at `position` whose +Z axis points along `direction`
        [[nodiscard]] XMFLOAT4X4 MarkerWorld(const XMFLOAT3 &position, const XMFLOAT3 &direction) {
            using namespace DirectX;

            const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&direction));
            // Any `up` works, except one along the direction itself
            const XMVECTOR up = std::abs(XMVectorGetY(forward)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                                                                        : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            // A view matrix turns the viewing direction onto +Z, so its inverse turns +Z onto `forward`.
            // The scale only sizes the sphere; the light ignores it.
            XMFLOAT4X4 world{};
            XMStoreFloat4x4(&world,
                            XMMatrixScaling(kMarkerRadius, kMarkerRadius, kMarkerRadius) *
                                XMMatrixInverse(nullptr, XMMatrixLookToLH(XMLoadFloat3(&position), forward, up)));
            return world;
        }

        // Unlit, so the sphere shows the light's own colour whatever shines on it
        [[nodiscard]] engine::render::MaterialHandle MarkerMaterial(engine::assets::AssetCache &cache,
                                                                    const XMFLOAT3 &color) {
            const auto toByte = [](float value) {
                return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            return cache.Material(cache.SolidColor(toByte(color.x), toByte(color.y), toByte(color.z)),
                                  engine::render::Shading::Unlit);
        }
    } // namespace

    void AddDefaultLights(engine::scene::Scene &scene, engine::assets::AssetCache &cache, float z) {
        const auto addLight = [&](const char *name, const LocalLight &light, const XMFLOAT3 &position,
                                  const XMFLOAT3 &direction) {
            scene.AddLight(name, cache.SphereModel(MarkerMaterial(cache, light.color)),
                           MarkerWorld({position.x, position.y, position.z + z}, direction), light);
        };
        const auto addPoint = [&](const char *name, const XMFLOAT3 &color, const XMFLOAT3 &position) {
            // A point light shines the same in every direction, so its sphere is not turned
            addLight(name, LocalLight{.type = LightType::Point, .color = color, .strength = 3.5f}, position,
                     {0.0f, 0.0f, 1.0f});
        };
        const auto addSpot = [&](const char *name, const XMFLOAT3 &color, const XMFLOAT3 &position) {
            addLight(name, LocalLight{.type = LightType::Spot, .color = color, .strength = 1.5f}, position,
                     kSpotDirection);
        };
        // Left and right behind the cubes, at their height: far enough apart for each to have its own pool
        addPoint("Point light 1", {0.0f, 0.0f, 1.0f}, {-6.0f, 0.0f, 6.0f});
        addPoint("Point light 2", {1.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 6.0f});
        // Further back and higher, shining onto the grid behind them
        addSpot("Spot light 1", {1.0f, 0.55f, 0.2f}, {5.0f, 2.0f, 12.0f});
        addSpot("Spot light 2", {0.0f, 1.0f, 0.0f}, {-5.0f, 2.0f, 12.0f});
    }

    void RecolorMarker(engine::scene::Scene &scene, engine::assets::AssetCache &cache, engine::scene::ObjectId id) {
        if (const LocalLight *const light = scene.Light(id)) {
            scene.SetObjectMaterial(id, MarkerMaterial(cache, light->color));
        }
    }
} // namespace sandbox
