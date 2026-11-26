#ifndef D3D12_ENGINE_TIMER_H
#define D3D12_ENGINE_TIMER_H

// Measures how long something takes, and the length and rate of the frames.

#include <chrono>
#include <cstdint>

namespace core {
    class Timer {
    public:
        using Clock = std::chrono::steady_clock;

        [[nodiscard]] double ElapsedMs() const;

    private:
        Clock::time_point m_start = Clock::now();
    };

    class FrameTimer {
    public:
        using Clock = std::chrono::steady_clock;

        void Tick();

        [[nodiscard]] double DeltaSeconds() const noexcept { return m_deltaSeconds; }

        [[nodiscard]] double TotalSeconds() const noexcept { return m_totalSeconds; }

        [[nodiscard]] double Fps() const noexcept { return m_fps; }

    private:
        Clock::time_point m_start = Clock::now();
        Clock::time_point m_last = m_start;
        double m_deltaSeconds = 0.0;
        double m_totalSeconds = 0.0;

        double m_fps = 0.0;
        double m_fpsWindowSeconds = 0.0;
        uint32_t m_fpsWindowFrames = 0;
    };
} // namespace core

#endif //D3D12_ENGINE_TIMER_H
