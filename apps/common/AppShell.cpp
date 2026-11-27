#include "AppShell.h"

#include <span>
#include <utility>

#include "core/Assert.h"

namespace fs = std::filesystem;

namespace apps {
    namespace {
        std::optional<ui::DebugUI> MakeDebugUi(bool enabled, const core::Window &window) {
            if (!enabled) {
                return std::nullopt;
            }
            return std::optional<ui::DebugUI>(std::in_place, window);
        }
    } // namespace

    AppShell::AppShell(const AppShellDesc &desc, rhi::ValidationStats &validation)
        : m_window({.title = desc.title}), m_ui(MakeDebugUi(desc.ui, m_window)),
          m_renderer(CreateRenderer(desc.fullscreen, validation)) {
        if (m_ui) {
            m_renderer.EnableUi();
        }
    }

    ui::DebugUI &AppShell::Ui() {
        ENGINE_ASSERT(m_ui);
        return *m_ui;
    }

    std::vector<DroppedFile> AppShell::TakeDroppedFiles() noexcept {
        return std::exchange(m_droppedFiles, {});
    }

    render::Renderer AppShell::CreateRenderer(bool fullscreen, rhi::ValidationStats &validation) {
        m_window.SetCallbacks({
            .onMessage =
                [this](const core::WindowMessage &message) {
                    const bool handledByUi = m_ui && ui::DebugUI::HandleMessage(message);
                    m_input.HandleMessage(message);
                    return handledByUi;
                },
            .onFilesDropped =
                [this](std::span<const fs::path> paths, int32_t x, int32_t y) {
                    for (const fs::path &path : paths) {
                        m_droppedFiles.push_back({.path = path, .x = x, .y = y});
                    }
                },
        });
        if (fullscreen) {
            m_window.SetFullscreen(true);
        }
        return render::Renderer(m_window.Handle(), m_window.Width(), m_window.Height(), validation);
    }
} // namespace apps
