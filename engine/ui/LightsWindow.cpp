#include "ui/LightsWindow.h"

#include <algorithm>

#include <imgui.h>

namespace ui {
    namespace {
        // Gap between the window and the main window's edges, before DPI scaling (as in DebugUI.cpp)
        constexpr float kPanelMargin = 10.0f;

        // Shows the fields of a point or spot light. Returns true in the frame a colour edit ends.
        // ImGui tells the widgets of a window apart by their labels. The hidden "##light" suffix keeps these
        // apart from the directional light's.
        bool EditLocalLight(render::LocalLight &light) {
            ImGui::Checkbox("Enabled##light", &light.enabled);
            ImGui::ColorEdit3("Colour##light", &light.color.x);
            // The light's colour changes live while the picker is dragged. This is true once, when the drag ends.
            const bool colorEditEnded = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::SliderFloat("Strength##light", &light.strength, 0.0f, 10.0f, "%.2f");
            // How fast the light fades with distance d: 1 / (constant + linear * d + quadratic * d * d)
            ImGui::DragFloat3("Attenuation", &light.attenuation.x, 0.001f, 0.0f, 10.0f, "%.4f");
            ImGui::SetItemTooltip("Constant, linear, quadratic");
            if (light.type == render::LightType::Spot) {
                // Full brightness inside the inner angle, none outside the outer one. Each slider pushes the other
                // along, so the inner angle never gets bigger than the outer.
                if (ImGui::SliderFloat("Inner angle", &light.innerAngleDegrees, 0.0f, 90.0f, "%.1f deg")) {
                    light.outerAngleDegrees = std::max(light.outerAngleDegrees, light.innerAngleDegrees);
                }
                if (ImGui::SliderFloat("Outer angle", &light.outerAngleDegrees, 0.0f, 90.0f, "%.1f deg")) {
                    light.innerAngleDegrees = std::min(light.innerAngleDegrees, light.outerAngleDegrees);
                }
                ImGui::TextDisabled("Move it with the gizmo (W); turn it with Rotate (E)");
            } else {
                ImGui::TextDisabled("Move it with the gizmo (W)");
            }
            return colorEditEnded;
        }
    } // namespace

    bool ShowLightsWindow(render::LightEnvironment &environment, std::string_view selectedName,
                          render::LocalLight *selected) {
        bool colorEditEnded = false;
        const float margin = kPanelMargin * ImGui::GetStyle().FontScaleDpi;
        // Keeps the window in the bottom-right corner, below the Selection window. Set every frame, because the
        // window's height changes with the selection.
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(displaySize.x - margin, displaySize.y - margin), ImGuiCond_Always,
                                ImVec2(1.0f, 1.0f));
        if (ImGui::Begin("Lights", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::SeparatorText("Ambient and specular");
            ImGui::ColorEdit3("Ambient colour", &environment.ambientColor.x);
            ImGui::SliderFloat("Ambient strength", &environment.ambientStrength, 0.0f, 1.0f, "%.2f");
            ImGui::ColorEdit3("Specular colour", &environment.specularColor.x);
            // Higher gives a smaller, sharper highlight. The slider is logarithmic, because high values look alike.
            ImGui::SliderFloat("Shininess", &environment.shininess, 1.0f, 512.0f, "%.0f", ImGuiSliderFlags_Logarithmic);

            ImGui::SeparatorText("Directional light");
            render::DirectionalLight &directional = environment.directional;
            ImGui::Checkbox("Enabled", &directional.enabled);
            // Where the light shines; any length
            ImGui::DragFloat3("Direction", &directional.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
            ImGui::ColorEdit3("Colour", &directional.color.x);
            ImGui::SliderFloat("Strength", &directional.strength, 0.0f, 5.0f, "%.2f");

            ImGui::SeparatorText("Selected light");
            if (selected != nullptr) {
                ImGui::TextUnformatted(selectedName.data(), selectedName.data() + selectedName.size());
                colorEditEnded = EditLocalLight(*selected);
            } else {
                ImGui::TextDisabled("Click a light's sphere to edit it");
            }
        }
        // End() even when Begin() returned false (window collapsed)
        ImGui::End();
        return colorEditEnded;
    }
} // namespace ui
