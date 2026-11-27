#include "GpuTimeCollector.h"

#include <algorithm>
#include <cstddef>
#include <format>

namespace bench {
    static_assert(kMaxGpuPasses == rhi::kMaxGpuScopes);

    GpuTimeCollector::GpuTimeCollector(std::span<FrameTiming> timings, uint64_t warmupFrames)
        : m_timings(timings), m_warmupFrames(warmupFrames), m_nextFrame(warmupFrames) {}

    void GpuTimeCollector::Add(const rhi::GpuFrameTimes &times) {
        const uint64_t endFrame = m_warmupFrames + m_timings.size();
        if (m_error || times.frame < m_nextFrame || times.frame >= endFrame) {
            return;
        }
        if (times.frame != m_nextFrame) {
            m_error = std::format("no GPU times for frame {}", m_nextFrame);
            return;
        }
        const std::span<const rhi::GpuScopeTime> passes = times.Scopes();
        if (m_nextFrame == m_warmupFrames) {
            for (const rhi::GpuScopeTime &pass : passes) {
                m_passes.emplace_back(pass.name);
            }
        } else if (!std::ranges::equal(passes, m_passes, {}, &rhi::GpuScopeTime::name)) {
            m_error = std::format("the frame graph's passes changed at frame {}", times.frame);
            return;
        }
        FrameTiming &timing = m_timings[times.frame - m_warmupFrames];
        timing.gpuTotalMs = times.totalMs;
        for (size_t pass = 0; pass < passes.size(); ++pass) {
            timing.gpuPassMs[pass] = passes[pass].ms;
        }
        ++m_nextFrame;
    }

    std::expected<void, std::string> GpuTimeCollector::Check() const {
        if (m_error) {
            return std::unexpected(*m_error);
        }
        if (m_nextFrame != m_warmupFrames + m_timings.size()) {
            return std::unexpected(std::format("no GPU times for frame {}", m_nextFrame));
        }
        return {};
    }
} // namespace bench
