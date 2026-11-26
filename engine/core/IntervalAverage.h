#ifndef D3D12_ENGINE_INTERVALAVERAGE_H
#define D3D12_ENGINE_INTERVALAVERAGE_H

// Averages a value over one-second windows, so numbers on screen change once a second instead of every frame.

#include <cstdint>

namespace core {
    class IntervalAverage {
    public:
        static constexpr double kIntervalSeconds = 1.0;

        void Add(double value, double deltaSeconds);

        [[nodiscard]] double Value() const noexcept { return m_value; }

        void Reset() noexcept;

    private:
        double m_value = 0.0;
        double m_sum = 0.0;
        double m_seconds = 0.0;
        uint32_t m_count = 0;
    };
} // namespace core

#endif // D3D12_ENGINE_INTERVALAVERAGE_H
