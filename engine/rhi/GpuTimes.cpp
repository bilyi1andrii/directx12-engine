#include "rhi/GpuTimes.h"

#include <algorithm>
#include <cstddef>

namespace rhi {
    double TicksToMs(uint64_t begin, uint64_t end, uint64_t ticksPerSecond) noexcept {
        if (end < begin) {
            return 0.0;
        }
        return static_cast<double>(end - begin) * 1000.0 / static_cast<double>(ticksPerSecond);
    }

    GpuFrameTimes MakeGpuFrameTimes(uint64_t frame, std::span<const uint64_t> ticks,
                                    std::span<const std::string_view> names, uint64_t ticksPerSecond) {
        const auto scopeCount = static_cast<uint32_t>(names.size());
        GpuFrameTimes times{.frame = frame,
                            .totalMs =
                                TicksToMs(ticks[timestamp::kFrameBegin], ticks[timestamp::kFrameEnd], ticksPerSecond),
                            .scopeCount = scopeCount};
        for (uint32_t scope = 0; scope < scopeCount; ++scope) {
            times.scopes[scope] = {.name = names[scope],
                                   .ms = TicksToMs(ticks[timestamp::ScopeBegin(scope)],
                                                   ticks[timestamp::ScopeEnd(scope)], ticksPerSecond)};
        }
        return times;
    }

    uint32_t GpuScopeNames::Register(std::string_view name) {
        const auto found = std::ranges::find(m_names, name);
        if (found != m_names.end()) {
            return static_cast<uint32_t>(found - m_names.begin());
        }
        m_names.emplace_back(name);
        return static_cast<uint32_t>(m_names.size() - 1);
    }

    std::string_view GpuScopeNames::Name(uint32_t id) const {
        return m_names[id];
    }
} // namespace rhi
