#include "ui/LightsWindow.h"

#include <algorithm>

#include <imgui.h>

namespace ui {
    namespace {
        constexpr float kPanelMargin = 10.0f;

        bool EditLocalLight(render::LocalLight &light) {
            ImGui::Checkbox("Enabled##light", &light.enabled);
            ImGui::ColorEdit3("Colour##light", &light.color.x);
            const bool colorEditEnded = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::SliderFloat("Strength##light", &light.strength, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat3("Attenuation", &light.attenuation.x, 0.001f, 0.0f, 10.0f, "%.4f");
            ImGui::SetItemTooltip("Constant, linear, quadratic");
            if (light.type == render::LightType::Spot) {
                if (ImGui::SliderFloat("Inner angle", &light.innerAngleDegrees, 0.0f, 90.0f, "%.1f deg")) {
                    // keep inner <= outer
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
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(displaySize.x - margin, displaySize.y - margin), ImGuiCond_Always,
                                ImVec2(1.0f, 1.0f));
        if (ImGui::Begin("Lights", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::SeparatorText("Ambient and specular");
            ImGui::ColorEdit3("Ambient colour", &environment.ambientColor.x);
            ImGui::SliderFloat("Ambient strength", &environment.ambientStrength, 0.0f, 1.0f, "%.2f");
            ImGui::ColorEdit3("Specular colour", &environment.specularColor.x);
            ImGui::SliderFloat("Shininess", &environment.shininess, 1.0f, 512.0f, "%.0f", ImGuiSliderFlags_Logarithmic);

            ImGui::SeparatorText("Directional light");
            render::DirectionalLight &directional = environment.directional;
            ImGui::Checkbox("Enabled", &directional.enabled);
            // any length, normalized when packed
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
        ImGui::End();
        return colorEditEnded;
    }
} // namespace ui
