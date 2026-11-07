#ifndef D3D12_ENGINE_DEBUGUI_H
#define D3D12_ENGINE_DEBUGUI_H

#include "core/Window.h"

// ImGui types, opaque outside DebugUI.cpp and the renderer
struct ImDrawData;
struct ImGuiContext;

namespace engine::ui {
    // What the stats window shows
    struct FrameStats {
        double fps = 0.0; // 0 until the first measurement
    };

    // The debug UI (Dear ImGui). Owns the ImGui context and its Win32 platform backend (mouse, keyboard, cursor
    // shapes, display size) and builds the windows every frame. Drawing them is the renderer's job: pass Render()'s
    // result to Renderer::RenderUi.
    //
    // Per frame: NewFrame() -> Show*() -> Render().
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

        // A small window with the frame rate
        void ShowStats(const FrameStats &stats);

        // Ends the UI frame and returns what to draw, valid until the next NewFrame
        [[nodiscard]] ImDrawData *Render();

    private:
        ImGuiContext *m_context = nullptr;
    };
} // namespace engine::ui

#endif // D3D12_ENGINE_DEBUGUI_H
