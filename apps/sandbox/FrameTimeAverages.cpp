#include "FrameTimeAverages.h"

namespace sandbox {
    void FrameTimeAverages::Add(double deltaSeconds, double cpuRecordMs, const std::optional<rhi::GpuFrameTimes> &gpu) {
        m_cpuRecord.Add(cpuRecordMs, deltaSeconds);
        if (!gpu) {
            return;
        }
        m_gpuTotal.Add(gpu->totalMs, deltaSeconds);

        const std::span<const rhi::GpuScopeTime> passes = gpu->Scopes();
        if (passes.size() != m_passCount) {
            for (core::IntervalAverage &average : m_passAverages) {
                average.Reset();
            }
            m_passCount = passes.size();
        }
        for (size_t i = 0; i < passes.size(); ++i) {
            m_passAverages[i].Add(passes[i].ms, deltaSeconds);
            m_passes[i] = {.name = passes[i].name, .ms = m_passAverages[i].Value()};
        }
    }
} // namespace sandbox
