#ifndef D3D12_ENGINE_VALIDATIONSTATS_H
#define D3D12_ENGINE_VALIDATIONSTATS_H

// What the debug layers reported during a run: errors, leaked objects, and whether the checks ran at all. The app
// owns it and the smoke test reads it.

#include <atomic>
#include <cstdint>

namespace rhi {
    struct ValidationStats {
        std::atomic<uint32_t> errorCount{0};
        uint32_t liveObjectCount = 0;
        bool active = false;
    };
} // namespace rhi

#endif //D3D12_ENGINE_VALIDATIONSTATS_H
