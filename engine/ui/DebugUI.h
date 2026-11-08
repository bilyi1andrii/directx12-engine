#ifndef D3D12_ENGINE_DEBUGUI_H
#define D3D12_ENGINE_DEBUGUI_H

#include <cstddef>
#include <string_view>

#include <DirectXMath.h>

#include "core/Window.h"
#include "renderdata/RendererSettings.h"

// ImGui types, opaque outside DebugUI.cpp and the renderer
struct ImDrawData;
struct ImGuiContext;

namespace engine::ui {
    // What the renderer window shows about the current frame
    struct FrameStats {
        double fps = 0.0; // 0 until the first measurement
        size_t objectCount = 0;
        std::string_view sceneInfo; // one line about what is drawn, e.g. "grid 20 x 20, dynamic"
        DirectX::XMFLOAT3 cameraPosition{};
    };

    // The debug UI (Dear ImGui). Owns the ImGui context and its Win32 platform backend (mouse, keyboard, cursor
    // shapes, display size) and builds the windows every frame. Drawing them is the renderer's job: pass Render()'s
    // result to Renderer::RenderUi.
    //
    // Per frame: NewFrame() -> [TranslateGizmo()] -> Show*() -> Render().
    //
    // The UI only edits data (RendererSettings, an object's position); the app passes it on to the renderer, which
    // reads it once per frame. It never calls the renderer itself.
    //
    // ImGui keeps its current context in a global, so there is one DebugUI per process. Create it after the window
    // and before the renderer, and destroy it after the renderer (the renderer's ImGui backend uses the context).
    // Not copyable or movable.
    class DebugUI {
    public:
        explicit DebugUI(const core::Window &window);
        ~DebugUI();

        DebugUI(const DebugUI &) = delete;
        DebugUI &operator=(const DebugUI &) = delete;
        DebugUI(DebugUI &&) = delete;
        DebugUI &operator=(DebugUI &&) = delete;

        // Passes a window message to ImGui; call it for every message (Window's onMessage). Returns true if ImGui
        // handled the message and the window must not (WM_SETCURSOR: otherwise DefWindowProc resets ImGui's cursor
        // shape). Static and safe without a DebugUI: does nothing while no ImGui context exists, so the window may
        // outlive the UI.
        [[nodiscard]] static bool HandleMessage(const core::WindowMessage &message);

        // Starts the UI frame: reads the window's size and the input received since the last frame.
        // Must be followed by Render() in the same frame (ImGui asserts on two NewFrame calls in a row).
        void NewFrame();

        // From NewFrame on: the input ImGui uses this frame, which the app should then ignore.
        // Mouse: the cursor is over an ImGui window, or a drag started on one. Keyboard: a widget takes typed
        // input (e.g. Ctrl+click on a slider to type a value).
        [[nodiscard]] bool WantsMouse() const;
        [[nodiscard]] bool WantsKeyboard() const;

        // Like WantsMouse, for turning the camera: hovering a gizmo handle does not count
        [[nodiscard]] bool WantsMouseForCamera() const;

        // The translate gizmo (ImGuizmo) on the object placed by `world`: arrows along the world X/Y/Z axes, plus
        // plane handles and a centre handle that moves in the screen plane. `view` and `projection` are the
        // camera's DirectXMath matrices as they are (not transposed). While the user drags a handle it changes
        // `world`'s translation and returns true. At most once per frame, after NewFrame.
        bool TranslateGizmo(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT4X4 &projection,
                            DirectX::XMFLOAT4X4 &world);

        // After this frame's TranslateGizmo: the cursor is over one of its handles, or one is being dragged, so a
        // click belongs to the gizmo (not to selecting). False in a frame without a gizmo.
        [[nodiscard]] bool GizmoWantsMouse() const;

        // The "Renderer" window: frame stats, and the settings (draw path, vsync), edited in place
        void ShowRendererWindow(const FrameStats &stats, render::RendererSettings &settings);

        // The "Selection" window for a selected object. Returns true if the user edited `position` (movable only).
        [[nodiscard]] bool ShowSelectionWindow(std::string_view name, DirectX::XMFLOAT3 &position, bool movable);

        // The "Selection" window when nothing is selected
        void ShowEmptySelectionWindow();

        // Ends the UI frame and returns what to draw, valid until the next NewFrame
        [[nodiscard]] ImDrawData *Render();

    private:
        ImGuiContext *m_context = nullptr;
        float m_dpiScale = 1.0f;
        bool m_gizmoDrawn = false;            // the gizmo is on screen this frame
        bool m_gizmoHovered = false;          // the cursor is over a handle, not dragging it
        bool m_gizmoHoveredLastFrame = false; // last frame's value: ImGuizmo's claim on the mouse lags one frame
    };
} // namespace engine::ui

#endif // D3D12_ENGINE_DEBUGUI_H
