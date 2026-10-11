#ifndef D3D12_ENGINE_WINDOW_H
#define D3D12_ENGINE_WINDOW_H

#include <cstdint>
#include <exception>
#include <functional>
#include <string_view>

#include <Windows.h>

namespace engine::core
{
    struct WindowMessage
    {
        HWND hwnd;
        UINT message;
        WPARAM wParam;
        LPARAM lParam;
    };

    struct WindowDesc
    {
        std::wstring_view title = L"D3D12 Engine";
        uint32_t width = 1280; // client area
        uint32_t height = 720;
    };

    struct WindowCallbacks
    {
        // Sees every message before the window handles it.
        // Return true to skip the window's own handling
        std::function<bool(const WindowMessage&)> onMessage;

        // The client area changed size. Not called when minimized
        std::function<void(uint32_t width, uint32_t height)> onResize;
    };

    // The application's main window
    // Not copyable or movable
    class Window
    {
    public:
        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        void SetCallbacks(WindowCallbacks callbacks);

        // Handles all pending messages without blocking.
        // Returns false once window has been closed
        [[nodiscard]] bool PumpMessages();

        // Asks the window to close
        void Close();
        void SetTitle(std::wstring_view title);

        [[nodiscard]] HWND Handle() const noexcept { return m_hwnd; }
        [[nodiscard]] uint32_t Width() const noexcept { return m_width; }
        [[nodiscard]] uint32_t Height() const noexcept { return m_height; }
        [[nodiscard]] bool IsMinimized() const noexcept { return m_minimized; }

        // The code passed to PostQuitMessage.
        // Valid after PumpMessage() returned false

        [[nodiscard]] int ExitCode() const noexcept { return m_exitCode; }

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam,
                                        LPARAM lParam);
        LRESULT HandleMessage(const WindowMessage& message);

        HWND m_hwnd = nullptr;
        WindowCallbacks m_callbacks;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_minimized = false;
        int m_exitCode = 0;
        std::exception_ptr m_pendingException;
    };
} // namespace engine::core

#endif //D3D12_ENGINE_WINDOW_H
