#include "core/Input.h"

#include <windowsx.h>

#include "core/ThrowIfFailed.h"

namespace core {
    namespace {
        // set for right Ctrl and Alt
        constexpr LPARAM kExtendedKeyBit = LPARAM{1} << 24;

        // set on auto-repeat, key already down
        constexpr LPARAM kPreviousStateBit = LPARAM{1} << 30;

        uint8_t ResolveSidedKey(WPARAM wParam, LPARAM lParam) {
            const bool extended = (lParam & kExtendedKeyBit) != 0;
            switch (wParam) {
            case VK_SHIFT: {
                const UINT scanCode = (lParam >> 16) & 0xFF;
                return static_cast<uint8_t>(::MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));
            }
            case VK_CONTROL:
                return extended ? VK_RCONTROL : VK_LCONTROL;
            case VK_MENU:
                return extended ? VK_RMENU : VK_LMENU;
            default:
                return static_cast<uint8_t>(wParam);
            }
        }

        bool Test(const std::bitset<256> &keys, uint8_t key) {
            switch (key) {
            case VK_SHIFT:
                return keys[VK_LSHIFT] || keys[VK_RSHIFT];
            case VK_CONTROL:
                return keys[VK_LCONTROL] || keys[VK_RCONTROL];
            case VK_MENU:
                return keys[VK_LMENU] || keys[VK_RMENU];
            default:
                return keys[key];
            }
        }
    } // namespace

    Input::Input() {
        const RAWINPUTDEVICE mouse{
            .usUsagePage = 0x01,
            .usUsage = 0x02,
            .dwFlags = 0,
            .hwndTarget = nullptr,
        };
        ThrowIfWin32Failed(::RegisterRawInputDevices(&mouse, 1, sizeof(mouse)) != FALSE);
    }

    void Input::HandleMessage(const WindowMessage &message) {
        switch (message.message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if ((message.lParam & kPreviousStateBit) == 0) {
                Press(ResolveSidedKey(message.wParam, message.lParam));
            }
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            m_down.reset(ResolveSidedKey(message.wParam, message.lParam));
            break;
        case WM_KILLFOCUS:
            m_down.reset();
            break;
        case WM_MOUSEMOVE:
            m_mousePosition = {GET_X_LPARAM(message.lParam), GET_Y_LPARAM(message.lParam)};
            break;
        case WM_LBUTTONDOWN:
            OnMouseButton(message, VK_LBUTTON, true);
            break;
        case WM_LBUTTONUP:
            OnMouseButton(message, VK_LBUTTON, false);
            break;
        case WM_RBUTTONDOWN:
            OnMouseButton(message, VK_RBUTTON, true);
            break;
        case WM_RBUTTONUP:
            OnMouseButton(message, VK_RBUTTON, false);
            break;
        case WM_MBUTTONDOWN:
            OnMouseButton(message, VK_MBUTTON, true);
            break;
        case WM_MBUTTONUP:
            OnMouseButton(message, VK_MBUTTON, false);
            break;
        case WM_MOUSEWHEEL:
            m_wheelDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(message.wParam)) / WHEEL_DELTA;
            break;
        case WM_INPUT:
            OnRawInput(message.lParam);
            break;
        default:
            break;
        }
    }

    void Input::EndFrame() {
        m_pressed.reset();
        m_rawMouseDelta = {};
        m_wheelDelta = 0.0f;
    }

    bool Input::IsKeyDown(uint8_t key) const {
        return Test(m_down, key);
    }

    bool Input::WasKeyPressed(uint8_t key) const {
        return Test(m_pressed, key);
    }

    void Input::Press(uint8_t key) {
        m_down.set(key);
        m_pressed.set(key);
    }

    void Input::OnMouseButton(const WindowMessage &message, uint8_t button, bool down) {
        m_mousePosition = {GET_X_LPARAM(message.lParam), GET_Y_LPARAM(message.lParam)};
        if (down) {
            Press(button);
            ::SetCapture(message.hwnd);
        } else {
            m_down.reset(button);
            ::ReleaseCapture();
        }
    }

    void Input::OnRawInput(LPARAM lParam) {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        ::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
        if (raw.header.dwType == RIM_TYPEMOUSE && (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
            m_rawMouseDelta.x += raw.data.mouse.lLastX;
            m_rawMouseDelta.y += raw.data.mouse.lLastY;
        }
    }
} // namespace core
