#ifndef D3D12_ENGINE_GPUTIMECOLLECTOR_H
#define D3D12_ENGINE_GPUTIMECOLLECTOR_H

// Collects the GPU times of a benchmark's measured frames, which arrive a few frames late and in order. A run fails
// if a frame's times never arrive or the frame graph's passes change during it.

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "bench/BenchOutput.h"
#include "rhi/GpuTimes.h"

namespace bench {
    class GpuTimeCollector {
    public:
        GpuTimeCollector(std::span<FrameTiming> timings, uint64_t warmupFrames);

        void Add(const rhi::GpuFrameTimes &times);

        [[nodiscard]] std::expected<void, std::string> Check() const;

        [[nodiscard]] std::span<const std::string> Passes() const noexcept { return m_passes; }

    private:
        std::span<FrameTiming> m_timings;
        uint64_t m_warmupFrames;
        uint64_t m_nextFrame;
        std::vector<std::string> m_passes;
        std::optional<std::string> m_error;
    };
} // namespace bench

#endif // D3D12_ENGINE_GPUTIMECOLLECTOR_H
