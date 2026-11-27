#ifndef D3D12_ENGINE_APPSHELL_H
#define D3D12_ENGINE_APPSHELL_H

// What every app runs in: the window, its keyboard and mouse input, the debug UI if the app wants one, and the
// renderer. It builds them in the order they depend on each other and keeps files dropped on the window for the app.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "core/Input.h"
#include "core/Window.h"
#include "render/Renderer.h"
#include "rhi/ValidationStats.h"
#include "ui/DebugUI.h"

namespace apps {
    struct AppShellDesc {
        std::wstring_view title;
        bool fullscreen = false;
        bool ui = false;
    };

    struct DroppedFile {
        std::filesystem::path path;
        int32_t x = 0; // client-area pixels
        int32_t y = 0;
    };

    class AppShell {
    public:
        AppShell(const AppShellDesc &desc, rhi::ValidationStats &validation);

        AppShell(const AppShell &) = delete;
        AppShell &operator=(const AppShell &) = delete;
        AppShell(AppShell &&) = delete;
        AppShell &operator=(AppShell &&) = delete;

        [[nodiscard]] core::Input &Input() noexcept { return m_input; }
        [[nodiscard]] core::Window &Window() noexcept { return m_window; }
        [[nodiscard]] ui::DebugUI &Ui();
        [[nodiscard]] render::Renderer &Renderer() noexcept { return m_renderer; }

        [[nodiscard]] std::vector<DroppedFile> TakeDroppedFiles() noexcept;

    private:
        [[nodiscard]] render::Renderer CreateRenderer(bool fullscreen, rhi::ValidationStats &validation);

        core::Input m_input;
        std::vector<DroppedFile> m_droppedFiles;
        core::Window m_window;
        // After the window (its backend needs the HWND), before the renderer (its ImGui backend needs the context)
        std::optional<ui::DebugUI> m_ui;
        render::Renderer m_renderer;
    };
} // namespace apps

#endif // D3D12_ENGINE_APPSHELL_H
