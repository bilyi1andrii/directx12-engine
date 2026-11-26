#ifndef D3D12_ENGINE_GPUTIMES_H
#define D3D12_ENGINE_GPUTIMES_H

// Turns GPU timestamps into milliseconds per frame and per pass, 
// and keeps pass names alive for results that arrive later.

#include <array>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>

namespace rhi {
    inline constexpr uint32_t kMaxGpuScopes = 16;

    struct GpuScopeTime {
        std::string_view name;
        double ms = 0.0;
    };

    struct GpuFrameTimes {
        uint64_t frame = 0;
        double totalMs = 0.0;
        std::array<GpuScopeTime, kMaxGpuScopes> scopes{};
        uint32_t scopeCount = 0;

        [[nodiscard]] std::span<const GpuScopeTime> Scopes() const noexcept { return {scopes.data(), scopeCount}; }
    };

    namespace timestamp {
        inline constexpr uint32_t kFrameBegin = 0;
        inline constexpr uint32_t kFrameEnd = 1;

        [[nodiscard]] constexpr uint32_t ScopeBegin(uint32_t scope) noexcept {
            return 2 + 2 * scope;
        }

        [[nodiscard]] constexpr uint32_t ScopeEnd(uint32_t scope) noexcept {
            return 3 + 2 * scope;
        }

        [[nodiscard]] constexpr uint32_t Count(uint32_t scopeCount) noexcept {
            return 2 + 2 * scopeCount;
        }

        inline constexpr uint32_t kPerFrame = Count(kMaxGpuScopes);
    } // namespace timestamp

    [[nodiscard]] double TicksToMs(uint64_t begin, uint64_t end, uint64_t ticksPerSecond) noexcept;

    [[nodiscard]] GpuFrameTimes MakeGpuFrameTimes(uint64_t frame, std::span<const uint64_t> ticks,
                                                  std::span<const std::string_view> names, uint64_t ticksPerSecond);

    class GpuScopeNames {
    public:
        [[nodiscard]] uint32_t Register(std::string_view name);

        [[nodiscard]] std::string_view Name(uint32_t id) const;

    private:
        std::deque<std::string> m_names;
    };
} // namespace rhi

#endif // D3D12_ENGINE_GPUTIMES_H
