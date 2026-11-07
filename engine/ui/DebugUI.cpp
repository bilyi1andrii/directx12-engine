#include "ui/DebugUI.h"

#include <array>
#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_win32.h>
// After imgui.h: ImGuizmo.h uses its types
#include <ImGuizmo.h>

// imgui_impl_win32.h declares it only inside "#if 0", so that the header does not need <windows.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace engine::ui {
    DebugUI::DebugUI(const core::Window &window) {
        // Fails if the headers and the compiled library are different ImGui versions
        IMGUI_CHECKVERSION();
        m_context = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        // ImGui would save window positions to imgui.ini in the working directory
        io.IniFilename = nullptr;
        // Keyboard navigation stays off: with it, a focused ImGui window keeps WantCaptureKeyboard true and the
        // camera keys would stop working after a click on a panel

        ImGui::StyleColorsDark();
        // The window is per-monitor DPI aware, so nothing scales the UI for us: 1.0 at 96 DPI, 1.5 at 144 DPI
        const float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(window.Handle());
        ImGuiStyle &style = ImGui::GetStyle();
        style.ScaleAllSizes(dpiScale); // paddings, rounding, borders
        style.FontScaleDpi = dpiScale; // fonts are rasterized at the scaled size (no blurry upscaling)

        if (!ImGui_ImplWin32_Init(window.Handle())) {
            ImGui::DestroyContext(m_context);
            throw std::runtime_error("ImGui: the Win32 backend failed to initialize");
        }
    }

    DebugUI::~DebugUI() {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(m_context);
    }

    bool DebugUI::HandleMessage(const core::WindowMessage &message) {
        return ImGui_ImplWin32_WndProcHandler(message.hwnd, message.message, message.wParam, message.lParam) != 0;
    }

    void DebugUI::NewFrame() {
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        // Before any other window: creates the invisible full-screen window the gizmo draws into. Created first,
        // it stays behind the panels, and it takes no input itself (ImGuiWindowFlags_NoInputs).
        ImGuizmo::BeginFrame();
        m_gizmoDrawn = false;
    }

    bool DebugUI::WantsMouse() const {
        // Set by NewFrame from the input received so far
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugUI::WantsKeyboard() const {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool DebugUI::TranslateGizmo(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT4X4 &projection,
                                 DirectX::XMFLOAT4X4 &world) {
        m_gizmoDrawn = true;
        // The whole client area, in the pixels ImGui uses (the same as the back buffer)
        const ImGuiIO &io = ImGui::GetIO();
        ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
        // By default ImGuizmo turns an arrow around when it points away from the camera (it looks shorter then);
        // keep every arrow on the +X/+Y/+Z side, so the arrows also show which way the world axes go
        ImGuizmo::AllowAxisFlip(false);
        // XMFLOAT4X4 stores 16 floats row by row, for row vectors (v * M): the layout ImGuizmo reads, so no
        // transpose (unlike the shader constants). ImGuizmo works with our left-handed projection and [0, 1] depth.
        return ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                                    &world.m[0][0]);
    }

    bool DebugUI::GizmoWantsMouse() const {
        // ImGuizmo keeps the last Manipulate's state, so without a gizmo this frame it would describe an old one
        return m_gizmoDrawn && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
    }

    void DebugUI::ShowRendererWindow(const FrameStats &stats, render::RendererSettings &settings) {
        const float scale = ImGui::GetStyle().FontScaleDpi;
        ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (stats.fps > 0.0) {
                ImGui::Text("%.1f FPS (%.2f ms)", stats.fps, 1000.0 / stats.fps);
            } else {
                ImGui::TextUnformatted("Measuring...");
            }
            ImGui::Text("%zu objects", stats.objectCount);
            if (!stats.scene.empty()) {
                ImGui::TextUnformatted(stats.scene.data(), stats.scene.data() + stats.scene.size());
            }
            ImGui::Text("Camera at %.2f, %.2f, %.2f", stats.cameraPosition.x, stats.cameraPosition.y,
                        stats.cameraPosition.z);

            // The keyboard shortcuts in brackets do the same
            ImGui::SeparatorText("Settings");
            // In DrawPath order: the combo's index is the enum's value
            constexpr std::array<const char *, 2> kDrawPaths{"Instanced", "Non-instanced"};
            int drawPath = static_cast<int>(settings.drawPath);
            if (ImGui::Combo("Draw path (I)", &drawPath, kDrawPaths.data(), static_cast<int>(kDrawPaths.size()))) {
                settings.drawPath = static_cast<render::DrawPath>(drawPath);
            }
            ImGui::Checkbox("VSync (V)", &settings.vsync);
        }
        // End() even when Begin() returned false (window collapsed)
        ImGui::End();
    }

    void DebugUI::ShowSelectionWindow(std::string_view name, DirectX::XMFLOAT3 *position) {
        const float scale = ImGui::GetStyle().FontScaleDpi;
        // Top-right corner (pivot (1, 0): the position is the window's top-right corner), away from the objects in
        // the middle of the view. It widens when something is selected.
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10.0f * scale, 10.0f * scale),
                                ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
        if (ImGui::Begin("Selection", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (position == nullptr) {
                ImGui::TextDisabled("Left-click a coloured cube to select it");
            } else {
                ImGui::TextUnformatted(name.data(), name.data() + name.size());
                ImGui::DragFloat3("Position", &position->x, 0.05f, 0.0f, 0.0f, "%.2f");
                ImGui::TextDisabled("Drag the arrows to move it; click elsewhere to deselect");
            }
        }
        ImGui::End();
    }

    ImDrawData *DebugUI::Render() {
        ImGui::Render();
        return ImGui::GetDrawData();
    }
} // namespace engine::ui
