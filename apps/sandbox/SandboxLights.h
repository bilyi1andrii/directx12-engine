#ifndef D3D12_ENGINE_SANDBOXLIGHTS_H
#define D3D12_ENGINE_SANDBOXLIGHTS_H

#include "scene/Scene.h"

// The sandbox's lights
namespace sandbox {
    // Adds two point lights (blue, red) and two spot lights (orange, green) above the cubes, with the colours,
    // strengths and cone of the DX11 scene. `z` moves all four along Z, like the cubes when models are shown.
    // The ambient and directional light stay at the scene's defaults, which are the DX11 values too.
    void AddDefaultLights(engine::scene::Scene &scene, float z);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXLIGHTS_H
