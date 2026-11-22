#ifndef D3D12_ENGINE_INTERVALAVERAGE_H
#define D3D12_ENGINE_INTERVALAVERAGE_H

#include <cstdint>

namespace engine::core {
    class IntervalAverage {
    public:
        static constexpr double kDefaultIntervalSeconds = 1.0;

        IntervalAverage() = default;
        explicit IntervalAverage(double intervalSeconds) : m_intervalSeconds(intervalSeconds) {}

        void Add(double value, double deltaSeconds);

        [[nodiscard]] double Value() const noexcept { return m_value; }

        void Reset() noexcept;

    private:
        double m_intervalSeconds = kDefaultIntervalSeconds;
        double m_value = 0.0;
        double m_sum = 0.0;
        double m_seconds = 0.0;
        uint32_t m_count = 0;
    };
} // namespace engine::core

#endif // D3D12_ENGINE_INTERVALAVERAGE_H
