#include "core/Window.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <shellapi.h>

#include "core/ThrowIfFailed.h"

namespace core {
    namespace {
        constexpr wchar_t kClassName[] = L"EngineWindow";
        constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
        constexpr LONG kMinClientSize = 64;

        struct DropFinisher {
            void operator()(HDROP drop) const noexcept { ::DragFinish(drop); }
        };
        using UniqueDrop = std::unique_ptr<std::remove_pointer_t<HDROP>, DropFinisher>;

        std::vector<std::filesystem::path> DroppedPaths(HDROP drop) {
            const UINT count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::filesystem::path> paths;
            paths.reserve(count);
            for (UINT i = 0; i < count; ++i) {
                std::wstring path(::DragQueryFileW(drop, i, nullptr, 0), L'\0');
                ::DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size()) + 1);
                paths.emplace_back(std::move(path));
            }
            return paths;
        }
    } // namespace

    Window::Window(const WindowDesc &desc) {
        ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        const HINSTANCE instance = ::GetModuleHandleW(nullptr);

        const WNDCLASSEXW windowClass{
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = &Window::WndProc,
            .hInstance = instance,
            .hCursor = ::LoadCursorW(nullptr, IDC_ARROW),
            .lpszClassName = kClassName,
        };
        ThrowIfWin32Failed(::RegisterClassExW(&windowClass) != 0);

        RECT rect{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
        ThrowIfWin32Failed(::AdjustWindowRectExForDpi(&rect, kWindowedStyle, FALSE, 0, ::GetDpiForSystem()) != FALSE);

        const int windowWidth = rect.right - rect.left;
        const int windowHeight = rect.bottom - rect.top;
        const int x = (::GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
        const int y = (::GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

        const std::wstring title(desc.title);
        ::CreateWindowExW(0, kClassName, title.c_str(), kWindowedStyle, x, y, windowWidth, windowHeight, nullptr,
                          nullptr, instance, this);
        ThrowIfWin32Failed(m_hwnd != nullptr);

        ::DragAcceptFiles(m_hwnd, TRUE);

        RECT client{};
        ::GetClientRect(m_hwnd, &client);
        m_width = static_cast<uint32_t>(client.right);
        m_height = static_cast<uint32_t>(client.bottom);

        ::ShowWindow(m_hwnd, SW_SHOW);
    }

    Window::~Window() {
        m_callbacks = {};
        m_inDestructor = true;
        if (m_hwnd != nullptr) {
            ::DestroyWindow(m_hwnd);
        }
        ::UnregisterClassW(kClassName, ::GetModuleHandleW(nullptr));
    }

    void Window::SetCallbacks(WindowCallbacks callbacks) {
        m_callbacks = std::move(callbacks);
    }

    bool Window::PumpMessages() {
        MSG msg{};
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_exitCode = static_cast<int>(msg.wParam);
                return false;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);

            if (m_pendingException) {
                std::rethrow_exception(std::exchange(m_pendingException, nullptr));
            }
        }
        return true;
    }

    void Window::Close() {
        ::PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }

    void Window::SetFullscreen(bool fullscreen) {
        if (fullscreen == m_fullscreen) {
            return;
        }

        const LONG_PTR style = ::GetWindowLongPtrW(m_hwnd, GWL_STYLE);
        if (fullscreen) {
            MONITORINFO monitor{.cbSize = sizeof(MONITORINFO)};
            ThrowIfWin32Failed(::GetWindowPlacement(m_hwnd, &m_windowedPlacement) != FALSE);
            ThrowIfWin32Failed(::GetMonitorInfoW(::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &monitor) !=
                               FALSE);

            ::SetWindowLongPtrW(m_hwnd, GWL_STYLE, style & ~static_cast<LONG_PTR>(kWindowedStyle));
            const RECT &area = monitor.rcMonitor;
            ::SetWindowPos(m_hwnd, HWND_TOP, area.left, area.top, area.right - area.left, area.bottom - area.top,
                           SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        } else {
            ::SetWindowLongPtrW(m_hwnd, GWL_STYLE, style | kWindowedStyle);
            ::SetWindowPlacement(m_hwnd, &m_windowedPlacement);
            ::SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        m_fullscreen = fullscreen;
    }

    LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) {
            auto *created = static_cast<Window *>(reinterpret_cast<const CREATESTRUCTW *>(lParam)->lpCreateParams);
            created->m_hwnd = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        }

        auto *self = reinterpret_cast<Window *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self == nullptr) {
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }
        if (message == WM_NCDESTROY) {
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            self->m_hwnd = nullptr;
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }

        try {
            return self->HandleMessage({hwnd, message, wParam, lParam});
        } catch (...) {
            self->m_pendingException = std::current_exception();
            return 0;
        }
    }

    LRESULT Window::HandleMessage(const WindowMessage &msg) {
        if (m_callbacks.onMessage && m_callbacks.onMessage(msg)) {
            return 0;
        }

        switch (msg.message) {
        case WM_SIZE:
            m_minimized = msg.wParam == SIZE_MINIMIZED;
            if (!m_minimized) {
                m_width = LOWORD(msg.lParam);
                m_height = HIWORD(msg.lParam);
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto *info = reinterpret_cast<MINMAXINFO *>(msg.lParam);
            info->ptMinTrackSize = {kMinClientSize, kMinClientSize};
            return 0;
        }
        case WM_DPICHANGED: {
            const auto *suggested = reinterpret_cast<const RECT *>(msg.lParam);
            ::SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                           suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_DROPFILES: {
            const UniqueDrop drop(reinterpret_cast<HDROP>(msg.wParam));
            const std::vector<std::filesystem::path> paths = DroppedPaths(drop.get());
            POINT point{};
            ::DragQueryPoint(drop.get(), &point);
            if (m_callbacks.onFilesDropped) {
                m_callbacks.onFilesDropped(paths, point.x, point.y);
            }
            return 0;
        }
        case WM_SYSCHAR:
            if (msg.wParam == VK_RETURN) {
                return 0;
            }
            break;
        case WM_DESTROY:
            if (!m_inDestructor) {
                ::PostQuitMessage(0);
            }
            return 0;
        default:
            break;
        }
        return ::DefWindowProcW(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    }
} // namespace core
