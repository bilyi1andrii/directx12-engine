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
    namespace {
        // Gap between the panels and the window's edges, before DPI scaling
        constexpr float kPanelMargin = 10.0f;

        // Windows does not scale the UI for us: 1.0 at 96 DPI, 1.5 at 144 DPI
        void ApplyDpiScale(float dpiScale) {
            // ScaleAllSizes multiplies the current sizes, so start from a fresh style
            ImGuiStyle style;
            ImGui::StyleColorsDark(&style);
            style.ScaleAllSizes(dpiScale); // paddings, rounding, borders
            style.FontScaleDpi = dpiScale; // sharp text at the new size
            ImGui::GetStyle() = style;
        }

        // Must be followed by ImGui::End(), whatever it returns
        bool BeginSelectionWindow() {
            const float margin = kPanelMargin * ImGui::GetStyle().FontScaleDpi;
            // Keeps the window in the top-right corner. Set every frame because its width changes with its text.
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - margin, margin), ImGuiCond_Always,
                                    ImVec2(1.0f, 0.0f));
            return ImGui::Begin("Selection", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        }
    } // namespace

    DebugUI::DebugUI(const core::Window &window) {
        // Fails if the headers and the compiled library are different ImGui versions
        IMGUI_CHECKVERSION();
        m_context = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        // ImGui would save window positions to imgui.ini in the working directory
        io.IniFilename = nullptr;
        // Keyboard navigation stays off: with it, a focused ImGui window keeps WantCaptureKeyboard true and the
        // camera keys would stop working after a click on a panel

        m_dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(window.Handle());
        ApplyDpiScale(m_dpiScale);

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
        // The window may have moved to another monitor. Before NewFrame, which reads the font scale.
        const float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(ImGui::GetMainViewport()->PlatformHandleRaw);
        if (dpiScale != m_dpiScale) {
            m_dpiScale = dpiScale;
            ApplyDpiScale(dpiScale);
        }
        ImGui::NewFrame();
        // Before any other window: creates the invisible full-screen window the gizmo draws into. Created first,
        // it stays behind the panels, and it takes no input itself (ImGuiWindowFlags_NoInputs).
        ImGuizmo::BeginFrame();
        m_gizmoDrawn = false;
        m_gizmoHoveredLastFrame = m_gizmoHovered;
        m_gizmoHovered = false;
    }

    bool DebugUI::WantsMouse() const {
        // Set by NewFrame from the input received so far
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugUI::WantsMouseForCamera() const {
        // ImGuizmo claims the mouse whenever the cursor is over a handle. That should not stop the camera from turning.
        return ImGui::GetIO().WantCaptureMouse && !m_gizmoHoveredLastFrame;
    }

    bool DebugUI::WantsKeyboard() const {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool DebugUI::TransformGizmo(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT4X4 &projection,
                                 DirectX::XMFLOAT4X4 &world) {
        using namespace DirectX;

        // ImGuizmo hides the gizmo when clip z < 0.001: near the far plane with reversed depth, but not behind the
        // camera, where clip z is positive. Skip both cases here, so a hidden gizmo takes no clicks. A drag goes on.
        const XMVECTOR clip = XMVector4Transform(XMVectorSet(world._41, world._42, world._43, 1.0f),
                                                 XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));
        constexpr float kMinDistance = 0.001f;
        constexpr float kImGuizmoMinClipZ = 0.001f;
        if ((XMVectorGetW(clip) < kMinDistance || XMVectorGetZ(clip) < kImGuizmoMinClipZ) && !ImGuizmo::IsUsing()) {
            return false; // m_gizmoDrawn stays false from NewFrame
        }
        // The whole client area, in the pixels ImGui uses (the same as the back buffer)
        const ImGuiIO &io = ImGui::GetIO();
        ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
        // By default ImGuizmo turns an arrow around when it points away from the camera (it looks shorter then);
        // keep every arrow on the +X/+Y/+Z side, so the arrows also show which way the world axes go
        ImGuizmo::AllowAxisFlip(false);
        // XMFLOAT4X4 stores 16 floats row by row, for row vectors (v * M): the layout ImGuizmo reads, so no
        // transpose (unlike the shader constants). ImGuizmo works with our left-handed projection and reversed depth.
        const ImGuizmo::OPERATION operation = m_gizmoMode == GizmoMode::Rotate  ? ImGuizmo::ROTATE
                                              : m_gizmoMode == GizmoMode::Scale ? ImGuizmo::SCALE
                                                                                : ImGuizmo::TRANSLATE;
        const ImGuizmo::MODE space = m_gizmoMode == GizmoMode::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        const bool moved = ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, space, &world.m[0][0]);
        m_gizmoDrawn = true;
        m_gizmoHovered = !ImGuizmo::IsUsing() && ImGuizmo::IsOver();
        return moved;
    }

    bool DebugUI::GizmoWantsMouse() const {
        // ImGuizmo keeps the last Manipulate's state, so without a gizmo this frame it would describe an old one
        return m_gizmoDrawn && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
    }

    void DebugUI::ShowRendererWindow(const FrameStats &stats, render::RendererSettings &settings) {
        const float margin = kPanelMargin * ImGui::GetStyle().FontScaleDpi;
        ImGui::SetNextWindowPos(ImVec2(margin, margin), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (stats.fps > 0.0) {
                ImGui::Text("%.1f FPS (%.2f ms)", stats.fps, 1000.0 / stats.fps);
            } else {
                ImGui::TextUnformatted("Measuring...");
            }
            ImGui::Text("%zu objects", stats.objectCount);
            if (!stats.sceneInfo.empty()) {
                ImGui::TextUnformatted(stats.sceneInfo.data(), stats.sceneInfo.data() + stats.sceneInfo.size());
            }
            ImGui::Text("Camera at %.2f, %.2f, %.2f", stats.cameraPosition.x, stats.cameraPosition.y,
                        stats.cameraPosition.z);
            ImGui::Text("Camera speed x%.2f (right mouse button + wheel)", stats.cameraSpeedScale);

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

    bool DebugUI::ShowSelectionWindow(std::string_view name, DirectX::XMFLOAT3 &position, bool movable) {
        bool edited = false;
        if (BeginSelectionWindow()) {
            ImGui::TextUnformatted(name.data(), name.data() + name.size());
            if (movable) {
                int mode = static_cast<int>(m_gizmoMode);
                ImGui::RadioButton("Move (W)", &mode, static_cast<int>(GizmoMode::Translate));
                ImGui::SameLine();
                ImGui::RadioButton("Rotate (E)", &mode, static_cast<int>(GizmoMode::Rotate));
                ImGui::SameLine();
                ImGui::RadioButton("Scale (R)", &mode, static_cast<int>(GizmoMode::Scale));
                if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
                        mode = static_cast<int>(GizmoMode::Translate);
                    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                        mode = static_cast<int>(GizmoMode::Rotate);
                    } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                        mode = static_cast<int>(GizmoMode::Scale);
                    }
                }
                m_gizmoMode = static_cast<GizmoMode>(mode);
                edited = ImGui::DragFloat3("Position", &position.x, 0.05f, 0.0f, 0.0f, "%.2f");
                ImGui::TextDisabled("Drag the gizmo's handles; click elsewhere to deselect");
            } else {
                ImGui::Text("Position %.2f, %.2f, %.2f", position.x, position.y, position.z);
                ImGui::TextDisabled("Static: it cannot be moved. Click elsewhere to deselect");
            }
        }
        ImGui::End();
        return edited;
    }

    void DebugUI::ShowEmptySelectionWindow() {
        if (BeginSelectionWindow()) {
            ImGui::TextDisabled("Left-click a cube or a model to select it");
        }
        ImGui::End();
    }

    ImDrawData *DebugUI::Render() {
        ImGui::Render();
        return ImGui::GetDrawData();
    }
} // namespace engine::ui
