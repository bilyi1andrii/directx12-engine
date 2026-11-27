#ifndef D3D12_ENGINE_DEBUGUI_H
#define D3D12_ENGINE_DEBUGUI_H

// The debug UI: it owns ImGui and builds the Renderer and Selection windows and the gizmo every frame. It only edits
// data the app hands it; the renderer draws the result.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <DirectXMath.h>

#include "core/Window.h"
#include "renderdata/RendererSettings.h"

struct ImDrawData;
struct ImGuiContext;

namespace ui {
    struct PassTime {
        std::string_view name;
        double ms = 0.0;
    };

    // what Renderer window shows, filled by app
    struct FrameStats {
        double fps = 0.0;
        double cpuRecordMs = 0.0;
        double gpuTotalMs = 0.0;
        std::span<const PassTime> gpuPasses;
        size_t objectCount = 0;
        std::string_view sceneInfo;
        DirectX::XMFLOAT3 cameraPosition{};
        float cameraSpeedScale = 1.0f;
    };

    class DebugUI {
    public:
        explicit DebugUI(const core::Window &window);
        ~DebugUI();

        DebugUI(const DebugUI &) = delete;
        DebugUI &operator=(const DebugUI &) = delete;
        DebugUI(DebugUI &&) = delete;
        DebugUI &operator=(DebugUI &&) = delete;

        // static, forwards to ImGui's current context
        [[nodiscard]] static bool HandleMessage(const core::WindowMessage &message);

        void NewFrame();

        [[nodiscard]] bool WantsMouse() const;
        [[nodiscard]] bool WantsKeyboard() const;

        // like WantsMouse, but hovering gizmo doesn't block camera
        [[nodiscard]] bool WantsMouseForCamera() const;

        // edits world in place, true if moved
        bool TransformGizmo(const DirectX::XMFLOAT4X4 &view, const DirectX::XMFLOAT4X4 &projection,
                            DirectX::XMFLOAT4X4 &world);

        // click on gizmo handle must not pick object behind it
        [[nodiscard]] bool GizmoWantsMouse() const;

        void ShowRendererWindow(const FrameStats &stats, render::RendererSettings &settings);

        // true if position edited
        [[nodiscard]] bool ShowSelectionWindow(std::string_view name, DirectX::XMFLOAT3 &position, bool movable);

        void ShowEmptySelectionWindow();

        [[nodiscard]] ImDrawData *Render();

    private:
        ImGuiContext *m_context = nullptr;
        float m_dpiScale = 1.0f;
        enum class GizmoMode : uint8_t { Translate, Rotate, Scale };
        GizmoMode m_gizmoMode = GizmoMode::Translate;
        bool m_gizmoDrawn = false;
        bool m_gizmoHovered = false;
        bool m_gizmoHoveredLastFrame = false; // camera updates before gizmo draws
    };
} // namespace ui

#endif // D3D12_ENGINE_DEBUGUI_H
