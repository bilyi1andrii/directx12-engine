#ifndef D3D12_ENGINE_FRAMETIMEAVERAGES_H
#define D3D12_ENGINE_FRAMETIMEAVERAGES_H

// The frame times the Renderer window shows: CPU record time, GPU time of the frame and of each pass, each averaged
// over an interval.

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "core/IntervalAverage.h"
#include "rhi/GpuTimes.h"
#include "ui/DebugUI.h"

namespace sandbox {
    class FrameTimeAverages {
    public:
        void Add(double deltaSeconds, double cpuRecordMs, const std::optional<rhi::GpuFrameTimes> &gpu);

        [[nodiscard]] double CpuRecordMs() const noexcept { return m_cpuRecord.Value(); }
        [[nodiscard]] double GpuTotalMs() const noexcept { return m_gpuTotal.Value(); }
        [[nodiscard]] std::span<const ui::PassTime> GpuPasses() const noexcept {
            return std::span(m_passes).first(m_passCount);
        }

    private:
        core::IntervalAverage m_cpuRecord;
        core::IntervalAverage m_gpuTotal;
        std::array<core::IntervalAverage, rhi::kMaxGpuScopes> m_passAverages;
        std::array<ui::PassTime, rhi::kMaxGpuScopes> m_passes{};
        size_t m_passCount = 0;
    };
} // namespace sandbox

#endif // D3D12_ENGINE_FRAMETIMEAVERAGES_H
