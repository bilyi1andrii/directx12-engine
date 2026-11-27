#include "ui/DebugUI.h"

#include <array>
#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_win32.h>
// After imgui.h: ImGuizmo.h uses its types
#include <ImGuizmo.h>

// imgui_impl_win32.h declares it only inside "#if 0", so that the header needs no <windows.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ui {
    namespace {
        constexpr float kPanelMargin = 10.0f;

        void ApplyDpiScale(float dpiScale) {
            ImGuiStyle style;
            ImGui::StyleColorsDark(&style);
            style.ScaleAllSizes(dpiScale);
            style.FontScaleDpi = dpiScale;
            ImGui::GetStyle() = style;
        }

        bool BeginSelectionWindow() {
            const float margin = kPanelMargin * ImGui::GetStyle().FontScaleDpi;
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - margin, margin), ImGuiCond_Always,
                                    ImVec2(1.0f, 0.0f));
            return ImGui::Begin("Selection", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        }
    } // namespace

    DebugUI::DebugUI(const core::Window &window) {
        IMGUI_CHECKVERSION();
        m_context = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr; // no imgui.ini next to exe
        // Keyboard navigation stays off: a focused panel would keep WantCaptureKeyboard and block the camera keys

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
        // window may move to another monitor
        const float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(ImGui::GetMainViewport()->PlatformHandleRaw);
        if (dpiScale != m_dpiScale) {
            m_dpiScale = dpiScale;
            ApplyDpiScale(dpiScale);
        }
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        m_gizmoDrawn = false;
        m_gizmoHoveredLastFrame = m_gizmoHovered;
        m_gizmoHovered = false;
    }

    bool DebugUI::WantsMouse() const {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugUI::WantsMouseForCamera() const {
        return ImGui::GetIO().WantCaptureMouse && !m_gizmoHoveredLastFrame;
    }

    bool DebugUI::WantsKeyboard() const {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool DebugUI::TransformGizmo(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT4X4 &projection,
                                 DirectX::XMFLOAT4X4 &world) {
        using namespace DirectX;

        const XMVECTOR clip = XMVector4Transform(XMVectorSet(world._41, world._42, world._43, 1.0f),
                                                 XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));
        constexpr float kMinDistance = 0.001f;
        constexpr float kImGuizmoMinClipZ = 0.001f; // ImGuizmo's own clip limit
        // skip if behind camera, unless dragging
        if ((XMVectorGetW(clip) < kMinDistance || XMVectorGetZ(clip) < kImGuizmoMinClipZ) && !ImGuizmo::IsUsing()) {
            return false;
        }
        const ImGuiIO &io = ImGui::GetIO();
        ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::AllowAxisFlip(false);
        const ImGuizmo::OPERATION operation = m_gizmoMode == GizmoMode::Rotate  ? ImGuizmo::ROTATE
                                              : m_gizmoMode == GizmoMode::Scale ? ImGuizmo::SCALE
                                                                                : ImGuizmo::TRANSLATE;
        // scale along object's own axes
        const ImGuizmo::MODE space = m_gizmoMode == GizmoMode::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        const bool moved = ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, space, &world.m[0][0]);
        m_gizmoDrawn = true;
        m_gizmoHovered = !ImGuizmo::IsUsing() && ImGuizmo::IsOver();
        return moved;
    }

    bool DebugUI::GizmoWantsMouse() const {
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
            if (stats.cpuRecordMs > 0.0) {
                ImGui::Text("CPU record %.3f ms", stats.cpuRecordMs);
            }
            if (stats.gpuTotalMs > 0.0) {
                ImGui::Text("GPU %.3f ms", stats.gpuTotalMs);
                for (const PassTime &pass : stats.gpuPasses) {
                    ImGui::BulletText("%.*s %.3f ms", static_cast<int>(pass.name.size()), pass.name.data(), pass.ms);
                }
            }
            ImGui::Text("%zu objects", stats.objectCount);
            ImGui::TextUnformatted(stats.sceneInfo.data(), stats.sceneInfo.data() + stats.sceneInfo.size());
            ImGui::Text("Camera at %.2f, %.2f, %.2f", stats.cameraPosition.x, stats.cameraPosition.y,
                        stats.cameraPosition.z);
            ImGui::Text("Camera speed x%.2f (right mouse button + wheel)", stats.cameraSpeedScale);

            ImGui::SeparatorText("Settings");
            constexpr std::array<const char *, 2> kDrawPaths{"Instanced", "Non-instanced"};
            int drawPath = static_cast<int>(settings.drawPath);
            if (ImGui::Combo("Draw path (I)", &drawPath, kDrawPaths.data(), static_cast<int>(kDrawPaths.size()))) {
                settings.drawPath = static_cast<render::DrawPath>(drawPath);
            }
            ImGui::Checkbox("VSync (V)", &settings.vsync);
        }
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
                // W, E, R switch mode unless typing or flying
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
} // namespace ui
