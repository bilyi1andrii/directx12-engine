#include "core/Window.h"

#include <algorithm>
#include <string>
#include <utility>

#include "core/ThrowIfFailed.h"

namespace engine::core
{
    namespace
    {
        constexpr wchar_t kClassName[] = L"EngineWindow";
        constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
        constexpr LONG kMinClientSize = 64; // keeps the swap chain away from tiny sizes
    } // namespace

    Window::Window(const WindowDesc& desc)
    {
        // Per-monitor DPI awareness
        ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        const HINSTANCE instance = ::GetModuleHandleW(nullptr);

        const WNDCLASSEXW windowClass{
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = &Window::WndProc,
            .hInstance = instance,
            .hCursor = ::LoadCursorW(nullptr, IDC_ARROW),
            // No background brush: D3D12 draws every pixel, and erasing first
            // would flicker while resizing.
            .lpszClassName = kClassName,
        };
        if (::RegisterClassExW(&windowClass) == 0 && ::GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS)
        {
            ThrowIfWin32Failed(false);
        }

        // Increase the rectangle by the frame and title bar
        RECT rect{
            0, 0, static_cast<LONG>(desc.width),
            static_cast<LONG>(desc.height)
        };

        ThrowIfWin32Failed(::AdjustWindowRectExForDpi(&rect, kWindowedStyle,
                                                      FALSE, 0, ::GetDpiForSystem()) != FALSE);

        const int windowWidth = rect.right - rect.left;
        const int windowHeight = rect.bottom - rect.top;
        const int x = std::max(0, (::GetSystemMetrics(SM_CXSCREEN) - windowWidth)
                               / 2);
        const int y = std::max(0, (::GetSystemMetrics(SM_CYSCREEN) - windowHeight)
                               / 2);

        const std::wstring title(desc.title);
        ::CreateWindowExW(0, kClassName, title.c_str(), kWindowedStyle, x, y,
                          windowWidth, windowHeight,
                          nullptr, nullptr, instance, this);
        ThrowIfWin32Failed(m_hwnd != nullptr);

        RECT client{};
        ::GetClientRect(m_hwnd, &client);
        m_width = static_cast<uint32_t>(client.right);
        m_height = static_cast<uint32_t>(client.bottom);

        ::ShowWindow(m_hwnd, SW_SHOW);
    }

    Window::~Window()
    {
        // DestroyWindow still sends messages (e.g. WM_DESTROY)
        m_callbacks = {};
        if (m_hwnd != nullptr)
        {
            ::DestroyWindow(m_hwnd);
        }
        ::UnregisterClassW(kClassName, ::GetModuleHandleW(nullptr));
    }

    void Window::SetCallbacks(WindowCallbacks callbacks)
    {
        m_callbacks = std::move(callbacks);
    }

    bool Window::PumpMessages()
    {
        MSG msg{};

        // PeekMessage returns immediately when the queue is empty
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_exitCode = static_cast<int>(msg.wParam);
                return false;
            }
            ::TranslateMessage(&msg); // turns key presses into WM_CHAR
            ::DispatchMessageW(&msg); // calls WndProc

            if (m_pendingException)
            {
                std::rethrow_exception(std::exchange(m_pendingException,
                                                     nullptr));
            }
        }
        return true;
    }

    void Window::Close()
    {
        ::PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }

    void Window::SetTitle(std::wstring_view title)
    {
        ::SetWindowTextW(m_hwnd, std::wstring(title).c_str());
    }

    LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            // CreateWindowExW pass 'this', store in the window's user data
            auto* created = static_cast<Window*>(reinterpret_cast<const
                CREATESTRUCTW*>(lParam)->lpCreateParams);
            created->m_hwnd = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(created));
        }

        auto* self = reinterpret_cast<Window*>(::GetWindowLongPtrW(hwnd,
                                                                   GWLP_USERDATA));
        if (self == nullptr) // a few messages arrive before WM_NCCREATE
        {
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }
        if (message == WM_NCDESTROY) // the last message a window receives
        {
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            self->m_hwnd = nullptr;
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }

        // Windows itself calls WndProc. An exception must not go through
        // Windows frames, store it and then rethrow.
        try
        {
            return self->HandleMessage({hwnd, message, wParam, lParam});
        }
        catch (...)
        {
            if (!self->m_pendingException)
            {
                self->m_pendingException = std::current_exception();
            }
            return 0;
        }
    }

    LRESULT Window::HandleMessage(const WindowMessage& msg)
    {
        if (m_callbacks.onMessage && m_callbacks.onMessage(msg))
        {
            return 0;
        }

        switch (msg.message)
        {
        case WM_SIZE:
            {
                m_minimized = msg.wParam == SIZE_MINIMIZED;
                const uint32_t width = LOWORD(msg.lParam);
                const uint32_t height = HIWORD(msg.lParam);
                if (!m_minimized && width > 0 && height > 0 && (width != m_width ||
                    height != m_height))
                {
                    m_width = width;
                    m_height = height;
                    if (m_callbacks.onResize)
                    {
                        m_callbacks.onResize(width, height);
                    }
                }
                return 0;
            }
        case WM_GETMINMAXINFO:
            {
                auto* info = reinterpret_cast<MINMAXINFO*>(msg.lParam);
                info->ptMinTrackSize = {kMinClientSize, kMinClientSize};
                return 0;
            }
        case WM_DPICHANGED:
            {
                // Move to a monitor with a different scale
                const auto* suggested = reinterpret_cast<const RECT*>(msg.lParam);
                ::SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top,
                               suggested->right - suggested->left,
                               suggested->bottom - suggested->top, SWP_NOZORDER |
                               SWP_NOACTIVATE);
                return 0;
            }
        case WM_DESTROY:
            ::PostQuitMessage(0); // makes PumpMessages() return false
            return 0;
        default:
            break;
        }
        return ::DefWindowProcW(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    }
} // namespace engine::core
