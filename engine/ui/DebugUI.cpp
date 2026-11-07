#include "ui/DebugUI.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_win32.h>

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
    }

    void DebugUI::ShowStats(const FrameStats &stats) {
        const float scale = ImGui::GetStyle().FontScaleDpi;
        ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (stats.fps > 0.0) {
                ImGui::Text("%.1f FPS (%.2f ms)", stats.fps, 1000.0 / stats.fps);
            } else {
                ImGui::TextUnformatted("Measuring...");
            }
        }
        // End() even when Begin() returned false (window collapsed)
        ImGui::End();
    }

    ImDrawData *DebugUI::Render() {
        ImGui::Render();
        return ImGui::GetDrawData();
    }
} // namespace engine::ui
