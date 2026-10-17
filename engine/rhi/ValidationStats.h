#ifndef D3D12_ENGINE_VALIDATIONSTATS_H
#define D3D12_ENGINE_VALIDATIONSTATS_H

#include <atomic>
#include <cstdint>

namespace engine::rhi {
    // What the D3D12 debug layer and the DXGI debug layer reported during a run (Debug builds).
    // Owned by the app and filled in by rhi::Device. Must outlive the Device: ~Device writes the
    // live-object count after the device is released.
    struct ValidationStats {
        // ERROR and CORRUPTION messages from D3D12 and DXGI.
        // Atomic: the D3D12 message callback runs on whichever thread triggers the message.
        std::atomic<uint32_t> errorCount{0};
        // Objects still alive after the device was released (one report message each); nonzero means a leak
        uint32_t liveObjectCount = 0;
        // True only if all checks ran: D3D12 message callback, DXGI info queue and live-object report.
        // Always false in Release builds, which have no debug layer.
        bool active = false;
    };
} // namespace engine::rhi

#endif //D3D12_ENGINE_VALIDATIONSTATS_H
