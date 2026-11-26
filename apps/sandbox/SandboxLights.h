#ifndef D3D12_ENGINE_SANDBOXLIGHTS_H
#define D3D12_ENGINE_SANDBOXLIGHTS_H

// The sandbox's lights: two point and two spot lights, each shown as a small sphere in its colour.

#include "assets/AssetCache.h"
#include "scene/Scene.h"

namespace sandbox {
    void AddDefaultLights(scene::Scene &scene, assets::AssetCache &cache, float z);

    void RecolorMarker(scene::Scene &scene, assets::AssetCache &cache, scene::ObjectId id);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXLIGHTS_H
