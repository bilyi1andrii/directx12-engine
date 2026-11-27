#include "core/Timer.h"

namespace chrono = std::chrono;

namespace core {
    double Timer::ElapsedMs() const {
        return chrono::duration<double, std::milli>(Clock::now() - m_start).count();
    }

    void FrameTimer::Tick() {
        const Clock::time_point now = Clock::now();
        m_deltaSeconds = chrono::duration<double>(now - m_last).count();
        m_totalSeconds = chrono::duration<double>(now - m_start).count();
        m_last = now;

        m_fpsWindowSeconds += m_deltaSeconds;
        ++m_fpsWindowFrames;

        if (m_fpsWindowSeconds >= 1.0) {
            m_fps = m_fpsWindowFrames / m_fpsWindowSeconds;
            m_fpsWindowSeconds = 0.0;
            m_fpsWindowFrames = 0;
        }
    }
} // namespace core
