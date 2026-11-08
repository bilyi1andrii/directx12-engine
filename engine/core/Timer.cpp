#include "core/Timer.h"

namespace engine::core {
    bool Timer::Start() {
        if (m_running) {
            return false;
        }
        m_start = Clock::now();
        m_running = true;
        return true;
    }

    bool Timer::Stop() {
        if (!m_running) {
            return false;
        }
        m_stop = Clock::now();
        m_running = false;
        return true;
    }

    void Timer::Restart() {
        m_start = Clock::now();
        m_running = true;
    }

    double Timer::ElapsedMs() const {
        const Clock::time_point end = m_running ? Clock::now() : m_stop;
        return std::chrono::duration<double, std::milli>(end - m_start).count();
    }

    double FrameTimer::Tick() {
        const Clock::time_point now = Clock::now();
        m_deltaSeconds = std::chrono::duration<double>(now - m_last).count();
        m_totalSeconds = std::chrono::duration<double>(now - m_start).count();
        m_last = now;
        ++m_frameCount;

        // Average over 1s windows
        m_fpsWindowSeconds += m_deltaSeconds;
        ++m_fpsWindowFrames;

        if (m_fpsWindowSeconds >= 1.0) {
            m_fps = m_fpsWindowFrames / m_fpsWindowSeconds;
            m_fpsWindowSeconds = 0.0;
            m_fpsWindowFrames = 0;
        }
        return m_deltaSeconds;
    }
} // namespace engine::core
