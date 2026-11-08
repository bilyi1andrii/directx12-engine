#ifndef D3D12_ENGINE_TIMER_H
#define D3D12_ENGINE_TIMER_H

#include <chrono>
#include <cstdint>

namespace engine::core {
    class Timer {
    public:
        using Clock = std::chrono::steady_clock;

        // Starts measuring if stopped. Returns false if already running
        bool Start();

        // Stops measuring. Returns false if it was not running
        bool Stop();

        // Starts measuring from now, whether running or not
        void Restart();

        // While running: time since Start()
        // After Stop(): time between start and stop
        [[nodiscard]] double ElapsedMs() const;

        [[nodiscard]] bool IsRunning() const noexcept { return m_running; }

    private:
        Clock::time_point m_start = Clock::now();
        Clock::time_point m_stop = m_start;
        bool m_running = false;
    };

    class FrameTimer {
    public:
        using Clock = std::chrono::steady_clock;

        // Advances one frame. Returns seconds since the previous Tick()
        double Tick();

        [[nodiscard]] double DeltaSeconds() const noexcept { return m_deltaSeconds; }

        [[nodiscard]] double TotalSeconds() const noexcept { return m_totalSeconds; }

        [[nodiscard]] uint64_t FrameCount() const noexcept { return m_frameCount; }

        // Frames per second
        [[nodiscard]] double Fps() const noexcept { return m_fps; }

    private:
        Clock::time_point m_start = Clock::now();
        Clock::time_point m_last = m_start;
        double m_deltaSeconds = 0.0;
        double m_totalSeconds = 0.0;
        uint64_t m_frameCount = 0;

        double m_fps = 0.0;
        double m_fpsWindowSeconds = 0.0;
        uint32_t m_fpsWindowFrames = 0;
    };
} // namespace engine::core

#endif //D3D12_ENGINE_TIMER_H
