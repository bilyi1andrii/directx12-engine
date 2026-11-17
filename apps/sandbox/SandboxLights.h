#ifndef D3D12_ENGINE_SANDBOXLIGHTS_H
#define D3D12_ENGINE_SANDBOXLIGHTS_H

#include "assets/AssetCache.h"
#include "scene/Scene.h"

// The sandbox's lights
namespace sandbox {
    // Adds two point lights (blue, red) and two spot lights (orange, green). Each is drawn as a small 
    // sphere in its colour, which can be clicked and moved like any object. `z` moves all four along Z,
    // like the cubes when models are shown.
    void AddDefaultLights(engine::scene::Scene &scene, engine::assets::AssetCache &cache, float z);

    // Gives the sphere of the light `id` its light's current colour. Does nothing if `id` is not a light.
    void RecolorMarker(engine::scene::Scene &scene, engine::assets::AssetCache &cache, engine::scene::ObjectId id);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXLIGHTS_H
