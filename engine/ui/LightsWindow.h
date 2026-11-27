#ifndef D3D12_ENGINE_LIGHTSWINDOW_H
#define D3D12_ENGINE_LIGHTSWINDOW_H

// The Lights window: it edits the ambient, specular and directional light, and the selected point or spot light.

#include <string_view>

#include "renderdata/Lights.h"

namespace ui {
    // returns true when colour edit ends, caller recolours marker once
    [[nodiscard]] bool ShowLightsWindow(render::LightEnvironment &environment, std::string_view selectedName,
                                        render::LocalLight *selected);
} // namespace ui

#endif // D3D12_ENGINE_LIGHTSWINDOW_H
