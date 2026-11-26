#ifndef D3D12_ENGINE_GPUPROFILER_H
#define D3D12_ENGINE_GPUPROFILER_H

// Measures how long the GPU spends on each frame and on each pass. The GPU writes timestamps, and the profiler reads
// them back two frames later, so it never waits for the GPU.

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/FrameContext.h"
#include "rhi/GpuTimes.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class CommandContext;
    class GpuProfiler {
    public:
        GpuProfiler(NativeDevice *device, Allocator &allocator, uint64_t ticksPerSecond);

        GpuProfiler(const GpuProfiler &) = delete;
        GpuProfiler &operator=(const GpuProfiler &) = delete;
        GpuProfiler(GpuProfiler &&) = delete;
        GpuProfiler &operator=(GpuProfiler &&) = delete;

        [[nodiscard]] uint32_t RegisterScope(std::string_view name) { return m_names.Register(name); }

        void BeginFrame(CommandContext &commands, uint64_t frame);

        [[nodiscard]] uint32_t BeginScope(CommandContext &commands, uint32_t scopeId);
        void EndScope(CommandContext &commands, uint32_t scopeIndex);

        void EndFrame(CommandContext &commands);

        [[nodiscard]] const std::optional<GpuFrameTimes> &Latest() const noexcept { return m_latest; }

        [[nodiscard]] std::vector<GpuFrameTimes> ReadRemaining();

    private:
        struct SlotRecord {
            uint64_t frame = 0;
            bool resolved = false;
            std::array<uint32_t, kMaxGpuScopes> scopeIds{};
            uint32_t scopeCount = 0;
        };

        [[nodiscard]] static uint32_t FirstQuery(uint32_t slot) noexcept { return slot * timestamp::kPerFrame; }

        [[nodiscard]] GpuFrameTimes ReadSlot(uint32_t slot, const SlotRecord &record) const;

        ComPtr<ID3D12QueryHeap> m_queryHeap;
        Allocation m_readback;
        uint64_t m_ticksPerSecond = 0;
        GpuScopeNames m_names;
        std::array<SlotRecord, kFramesInFlight> m_slots{};
        uint32_t m_slot = 0;
        std::optional<GpuFrameTimes> m_latest;
    };

    class ScopedGpuTimer {
    public:
        ScopedGpuTimer(CommandContext &commands, GpuProfiler &profiler, uint32_t scopeId)
            : m_commands(commands), m_profiler(profiler), m_scope(profiler.BeginScope(commands, scopeId)) {}
        ~ScopedGpuTimer() { m_profiler.EndScope(m_commands, m_scope); }

        ScopedGpuTimer(const ScopedGpuTimer &) = delete;
        ScopedGpuTimer &operator=(const ScopedGpuTimer &) = delete;
        ScopedGpuTimer(ScopedGpuTimer &&) = delete;
        ScopedGpuTimer &operator=(ScopedGpuTimer &&) = delete;

    private:
        CommandContext &m_commands;
        GpuProfiler &m_profiler;
        uint32_t m_scope;
    };
} // namespace rhi

#endif // D3D12_ENGINE_GPUPROFILER_H
