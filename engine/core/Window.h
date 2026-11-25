#ifndef D3D12_ENGINE_WINDOW_H
#define D3D12_ENGINE_WINDOW_H

// The application's window: it handles Windows' messages, resizing, borderless fullscreen and files dropped onto
// it, and passes every message on to the app first.

#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>

#include <Windows.h>

namespace core {
    struct WindowMessage {
        HWND hwnd;
        UINT message;
        WPARAM wParam;
        LPARAM lParam;
    };

    struct WindowDesc {
        std::wstring_view title = L"D3D12 Engine";
        uint32_t width = 1280; // client area
        uint32_t height = 720;
    };

    struct WindowCallbacks {
        std::function<bool(const WindowMessage &)> onMessage;
        std::function<void(std::span<const std::filesystem::path> paths, int32_t clientX, int32_t clientY)>
            onFilesDropped;
    };

    class Window {
    public:
        explicit Window(const WindowDesc &desc);
        ~Window();

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

        void SetCallbacks(WindowCallbacks callbacks);

        [[nodiscard]] bool PumpMessages();

        void Close();

        void SetFullscreen(bool fullscreen);
        void ToggleFullscreen() { SetFullscreen(!m_fullscreen); }

        [[nodiscard]] HWND Handle() const noexcept { return m_hwnd; }
        [[nodiscard]] uint32_t Width() const noexcept { return m_width; }
        [[nodiscard]] uint32_t Height() const noexcept { return m_height; }
        [[nodiscard]] bool IsMinimized() const noexcept { return m_minimized; }

        [[nodiscard]] int ExitCode() const noexcept { return m_exitCode; }

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(const WindowMessage &message);

        HWND m_hwnd = nullptr;
        WindowCallbacks m_callbacks;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_minimized = false;
        bool m_fullscreen = false;
        WINDOWPLACEMENT m_windowedPlacement{.length = sizeof(WINDOWPLACEMENT)};
        int m_exitCode = 0;
        bool m_inDestructor = false;
        std::exception_ptr m_pendingException;
    };
} // namespace core

#endif //D3D12_ENGINE_WINDOW_H
