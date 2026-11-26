#include "SandboxLights.h"

#include <cmath>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
#include "renderdata/Material.h"

namespace sandbox {
    namespace {
        using DirectX::XMFLOAT3;
        using DirectX::XMFLOAT4X4;
        using render::LightType;
        using render::LocalLight;

        constexpr XMFLOAT3 kSpotDirection{-0.3f, -1.0f, 1.2f};

        constexpr float kMarkerRadius = 0.25f;

        [[nodiscard]] XMFLOAT4X4 MarkerWorld(const XMFLOAT3 &position, const XMFLOAT3 &direction) {
            using namespace DirectX;

            const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&direction));
            const XMVECTOR up = std::abs(XMVectorGetY(forward)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                                                                        : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            XMFLOAT4X4 world{};
            XMStoreFloat4x4(&world,
                            XMMatrixScaling(kMarkerRadius, kMarkerRadius, kMarkerRadius) *
                                XMMatrixInverse(nullptr, XMMatrixLookToLH(XMLoadFloat3(&position), forward, up)));
            return world;
        }

        [[nodiscard]] render::MaterialHandle MarkerMaterial(assets::AssetCache &cache, const XMFLOAT3 &color) {
            return cache.Material({.albedo = cache.SolidColor(color), .shading = render::Shading::Unlit});
        }
    } // namespace

    void AddDefaultLights(scene::Scene &scene, assets::AssetCache &cache, float z) {
        const auto addLight = [&](const char *name, const LocalLight &light, const XMFLOAT3 &position,
                                  const XMFLOAT3 &direction) {
            scene.AddLight(name, cache.SphereModel(MarkerMaterial(cache, light.color)),
                           MarkerWorld({position.x, position.y, position.z + z}, direction), light);
        };
        const auto addPoint = [&](const char *name, const XMFLOAT3 &color, const XMFLOAT3 &position) {
            addLight(name, LocalLight{.type = LightType::Point, .color = color, .strength = 3.5f}, position,
                     {0.0f, 0.0f, 1.0f});
        };
        const auto addSpot = [&](const char *name, const XMFLOAT3 &color, const XMFLOAT3 &position) {
            addLight(name, LocalLight{.type = LightType::Spot, .color = color, .strength = 1.5f}, position,
                     kSpotDirection);
        };
        addPoint("Point light 1", {0.0f, 0.0f, 1.0f}, {-6.0f, 0.0f, 6.0f});
        addPoint("Point light 2", {1.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 6.0f});
        addSpot("Spot light 1", {1.0f, 0.55f, 0.2f}, {5.0f, 2.0f, 12.0f});
        addSpot("Spot light 2", {0.0f, 1.0f, 0.0f}, {-5.0f, 2.0f, 12.0f});
    }

    void RecolorMarker(scene::Scene &scene, assets::AssetCache &cache, scene::ObjectId id) {
        scene.SetObjectMaterial(id, MarkerMaterial(cache, scene.Light(id)->color));
    }
} // namespace sandbox
