#ifndef D3D12_ENGINE_INPUT_H
#define D3D12_ENGINE_INPUT_H

#include <bitset>
#include <cstdint>

#include "core/Window.h"

namespace engine::core {
    struct Int2 {
        int x = 0;
        int y = 0;
    };

    // Keyboard and mouse state
    class Input {
    public:
        // Registers for raw mouse input (WM_INPUT)
        Input();

        // Redirect every window message here
        void HandleMessage(const WindowMessage &message);

        // Call once per frame, after the frame has read the input
        void EndFrame();

        [[nodiscard]] bool IsKeyDown(uint8_t key) const;

        [[nodiscard]] bool WasKeyPressed(uint8_t key) const; // went down during this frame

        [[nodiscard]] bool WasKeyReleased(uint8_t key) const; // went up during this frame

        // Cursor position in client-area pixels (can be outside the window while a button is held).
        [[nodiscard]] Int2 MousePosition() const noexcept { return m_mousePosition; }

        // Raw mouse movement during this frame
        [[nodiscard]] Int2 RawMouseDelta() const noexcept { return m_rawMouseDelta; }

        // Wheel movement during this frame, in notches.
        [[nodiscard]] float WheelDelta() const noexcept { return m_wheelDelta; }

    private:
        void Press(uint8_t key);
        void Release(uint8_t key);
        void ReleaseAll();
        void OnMouseButton(const WindowMessage &message, uint8_t button, bool down);
        void OnRawInput(LPARAM lParam);

        std::bitset<256> m_down;
        std::bitset<256> m_pressed;
        std::bitset<256> m_released;
        Int2 m_mousePosition;
        Int2 m_rawMouseDelta;
        float m_wheelDelta = 0.0f;
    };
} // namespace engine::core

#endif //D3D12_ENGINE_INPUT_H
