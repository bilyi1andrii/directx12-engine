#include "core/Input.h"

#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM

#include "core/ThrowIfFailed.h"

namespace engine::core {
    namespace {
        // Bits of lParam in WM_KEYDOWN/WM_KEYUP
        constexpr LPARAM kExtendedKeyBit = LPARAM{1} << 24;   // right-hand Ctrl/Alt
        constexpr LPARAM kPreviousStateBit = LPARAM{1} << 30; // the key was already down: auto-repeat

        // Windows reports Shift, Ctrl and Alt without saying which side was used. Turn them into VK_L*/VK_R*.
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

        // Answers a query for `key` against one of the bitsets, treating
        // VK_SHIFT/VK_CONTROL/VK_MENU as "either side".
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
        // Raw input delivers WM_INPUT with the mouse's own movement counts.
        const RAWINPUTDEVICE mouse{
            .usUsagePage = 0x01, // HID "generic desktop" page
            .usUsage = 0x02,     // mouse
            .dwFlags = 0,
            .hwndTarget = nullptr,
        };
        ThrowIfWin32Failed(::RegisterRawInputDevices(&mouse, 1, sizeof(mouse)) != FALSE);
    }

    void Input::HandleMessage(const WindowMessage &message) {
        switch (message.message) {
        // WM_SYS* arrive instead of WM_KEY* while Alt is held (and for F10).
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if ((message.lParam & kPreviousStateBit) == 0) // ignore auto-repeat
            {
                Press(ResolveSidedKey(message.wParam, message.lParam));
            }
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            Release(ResolveSidedKey(message.wParam, message.lParam));
            break;
        case WM_KILLFOCUS:
            // Key-up messages go to the focused window.
            // Without this, keys held during Alt+Tab would stay "down".
            ReleaseAll();
            break;
        case WM_MOUSEMOVE:
            // GET_X_LPARAM
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
        m_released.reset();
        m_rawMouseDelta = {};
        m_wheelDelta = 0.0f;
    }

    bool Input::IsKeyDown(uint8_t key) const {
        return Test(m_down, key);
    }

    bool Input::WasKeyPressed(uint8_t key) const {
        return Test(m_pressed, key);
    }

    bool Input::WasKeyReleased(uint8_t key) const {
        return Test(m_released, key);
    }

    void Input::Press(uint8_t key) {
        m_down.set(key);
        m_pressed.set(key);
    }

    void Input::Release(uint8_t key) {
        if (m_down.test(key)) {
            m_down.reset(key);
            m_released.set(key);
        }
    }

    void Input::ReleaseAll() {
        m_released |= m_down;
        m_down.reset();
    }

    void Input::OnMouseButton(const WindowMessage &message, uint8_t button, bool down) {
        m_mousePosition = {GET_X_LPARAM(message.lParam), GET_Y_LPARAM(message.lParam)};
        if (down) {
            Press(button);
            // Capture: keep receiving mouse messages while the button is held, even outside the window,
            // so the matching button-up is never lost.
            ::SetCapture(message.hwnd);
        } else {
            Release(button);
            if (!m_down.test(VK_LBUTTON) && !m_down.test(VK_RBUTTON) && !m_down.test(VK_MBUTTON)) {
                ::ReleaseCapture();
            }
        }
    }

    void Input::OnRawInput(LPARAM lParam) {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1)) {
            return;
        }
        // Ignore absolute-position devices (remote desktop, pen tablets)
        if (raw.header.dwType == RIM_TYPEMOUSE && (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
            m_rawMouseDelta.x += raw.data.mouse.lLastX;
            m_rawMouseDelta.y += raw.data.mouse.lLastY;
        }
    }
} // namespace engine::core
