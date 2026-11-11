#include "SandboxLights.h"

#include <cmath>

#include <DirectXMath.h>

#include "renderdata/Lights.h"
#include "renderdata/Model.h"

namespace sandbox {
    namespace {
        using DirectX::XMFLOAT3;
        using DirectX::XMFLOAT4X4;
        using engine::render::LightType;
        using engine::render::LocalLight;

        // Where both spot lights point: down and away from the camera's starting place
        constexpr XMFLOAT3 kSpotDirection{-0.3f, -1.0f, 1.2f};

        // The world matrix of an object at `position` whose +Z axis points along `direction`
        [[nodiscard]] XMFLOAT4X4 FacingWorld(const XMFLOAT3 &position, const XMFLOAT3 &direction) {
            using namespace DirectX;

            const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&direction));
            // Any `up` works, except one along the direction itself
            const XMVECTOR up = std::abs(XMVectorGetY(forward)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                                                                        : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            // A camera's view matrix turns its viewing direction onto +Z; the inverse turns +Z onto the direction
            XMFLOAT4X4 world{};
            XMStoreFloat4x4(&world, XMMatrixInverse(nullptr, XMMatrixLookToLH(XMLoadFloat3(&position), forward, up)));
            return world;
        }
    } // namespace

    void AddDefaultLights(engine::scene::Scene &scene, float z) {
        // Nothing to draw or click yet: the lights get their marker spheres later
        const engine::render::Model noMarker;

        const auto addPoint = [&](const char *name, const XMFLOAT3 &color, const XMFLOAT3 &position) {
            XMFLOAT4X4 world{};
            DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(position.x, position.y, position.z + z));
            scene.AddLight(name, noMarker, world,
                           LocalLight{.type = LightType::Point, .color = color, .strength = 3.5f});
        };
        const auto addSpot = [&](const char *name, const XMFLOAT3 &color, const XMFLOAT3 &position) {
            scene.AddLight(name, noMarker, FacingWorld({position.x, position.y, position.z + z}, kSpotDirection),
                           LocalLight{.type = LightType::Spot, .color = color, .strength = 1.5f});
        };
        // Left and right behind the cubes, at their height: far enough apart for each to have its own pool
        addPoint("Point light 1", {0.0f, 0.0f, 1.0f}, {-6.0f, 0.0f, 6.0f});
        addPoint("Point light 2", {1.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 6.0f});
        // Further back and higher, shining onto the grid behind them
        addSpot("Spot light 1", {1.0f, 0.55f, 0.2f}, {5.0f, 2.0f, 12.0f});
        addSpot("Spot light 2", {0.0f, 1.0f, 0.0f}, {-5.0f, 2.0f, 12.0f});
    }
} // namespace sandbox
