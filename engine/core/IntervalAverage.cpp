#include "core/IntervalAverage.h"

namespace core {
    void IntervalAverage::Add(double value, double deltaSeconds) {
        m_sum += value;
        m_seconds += deltaSeconds;
        ++m_count;
        if (m_seconds >= m_intervalSeconds) {
            m_value = m_sum / m_count;
            m_sum = 0.0;
            m_seconds = 0.0;
            m_count = 0;
        }
    }

    void IntervalAverage::Reset() noexcept {
        m_value = 0.0;
        m_sum = 0.0;
        m_seconds = 0.0;
        m_count = 0;
    }
} // namespace core
