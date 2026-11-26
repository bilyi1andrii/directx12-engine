#ifndef D3D12_ENGINE_LIGHTSWINDOW_H
#define D3D12_ENGINE_LIGHTSWINDOW_H

#include <string_view>

#include "renderdata/Lights.h"

namespace ui {
    // The "Lights" window: edits `environment` (ambient, specular, the directional light) and, if the selected
    // object is a light, that light (`selected`; nullptr otherwise). `selectedName` is the object's name.
    // Where a point or spot light stands and points is not edited here: the gizmo moves and turns its object.
    //
    // Returns true in the frame an edit of the selected light's colour ends (mouse released, or Enter after
    // typing), so the caller can update what shows the colour once and not on every frame of a drag.
    //
    // Call it between DebugUI::NewFrame and DebugUI::Render.
    [[nodiscard]] bool ShowLightsWindow(render::LightEnvironment &environment, std::string_view selectedName,
                                        render::LocalLight *selected);
} // namespace ui

#endif // D3D12_ENGINE_LIGHTSWINDOW_H
