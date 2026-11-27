#ifndef D3D12_ENGINE_INPUT_H
#define D3D12_ENGINE_INPUT_H

// The keyboard and mouse state of the current frame, built from the window's messages.

#include <bitset>
#include <cstdint>

#include "core/Window.h"

namespace core {
    struct Int2 {
        int x = 0;
        int y = 0;
    };

    class Input {
    public:
        Input();

        void HandleMessage(const WindowMessage &message);

        void EndFrame();

        [[nodiscard]] bool IsKeyDown(uint8_t key) const;

        [[nodiscard]] bool WasKeyPressed(uint8_t key) const;

        [[nodiscard]] Int2 MousePosition() const noexcept { return m_mousePosition; } // client-area pixels

        [[nodiscard]] Int2 RawMouseDelta() const noexcept { return m_rawMouseDelta; }

        [[nodiscard]] float WheelDelta() const noexcept { return m_wheelDelta; } // notches

    private:
        void Press(uint8_t key);
        void OnMouseButton(const WindowMessage &message, uint8_t button, bool down);
        void OnRawInput(LPARAM lParam);

        std::bitset<256> m_down; // held down
        std::bitset<256> m_pressed; // went down this frame
        Int2 m_mousePosition;
        Int2 m_rawMouseDelta;
        float m_wheelDelta = 0.0f;
    };
} // namespace core

#endif //D3D12_ENGINE_INPUT_H
