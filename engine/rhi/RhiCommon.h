#ifndef D3D12_ENGINE_RHICOMMON_H
#define D3D12_ENGINE_RHICOMMON_H

// Common includes and aliases for engine/rhi. Only rhi and render may include this header.

#include <directx/d3d12.h>
#include <dxgi1_6.h> // from the Windows SDK
#include <wrl/client.h>

namespace rhi {
    using Microsoft::WRL::ComPtr;

    // The device interface version the engine uses. Newer interfaces add methods
    // (ID3D12Device4: CreateCommandList1, ID3D12Device5: RemoveDevice for testing)
    using NativeDevice = ID3D12Device5;
} // namespace rhi

#endif //D3D12_ENGINE_RHICOMMON_H
