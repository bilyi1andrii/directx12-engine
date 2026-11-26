#ifndef D3D12_ENGINE_RHICOMMON_H
#define D3D12_ENGINE_RHICOMMON_H

// The D3D12 and DXGI headers and the device interface version that the rest of rhi and render use. Only those two
// modules may include it.

#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace rhi {
    using Microsoft::WRL::ComPtr;

    using NativeDevice = ID3D12Device5;
} // namespace rhi

#endif //D3D12_ENGINE_RHICOMMON_H
