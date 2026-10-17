#include "rhi/DepthBuffer.h"

#include <utility>

#include "rhi/FrameContext.h"

namespace engine::rhi {
    DepthBuffer::DepthBuffer(NativeDevice *device, Allocator &allocator, DescriptorAllocator &dsvAllocator,
                             uint32_t width, uint32_t height, std::wstring_view name)
        : m_device(device), m_allocator(&allocator), m_dsv(dsvAllocator.Allocate()), m_width(width), m_height(height),
          m_name(name) {
        Create();
    }

    void DepthBuffer::Resize(uint32_t width, uint32_t height, FrameContext &frame) {
        if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
            return;
        }

        // Frames still in flight may be testing against the old texture. Overwriting the DSV slot is
        // fine: CPU-only descriptors are copied into the command list when a command is recorded.
        frame.DeferRelease(std::move(m_allocation));
        m_width = width;
        m_height = height;
        Create();
    }

    void DepthBuffer::Create() {
        const D3D12_RESOURCE_DESC desc{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0, // default (64 KB)
            .Width = m_width,
            .Height = m_height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = kFormat,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, // the driver picks the (tiled) memory layout
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        };

        // Optimized clear value: GPUs keep compressed depth data and can clear it almost for free,
        // but only to the value given at creation. Clearing to any other value works, but is slower
        // and the debug layer warns about it.
        const D3D12_CLEAR_VALUE clearValue{
            .Format = kFormat,
            .DepthStencil = {.Depth = kClearDepth, .Stencil = 0},
        };

        // Default heap: GPU-local memory the CPU cannot access, where render targets and depth live.
        // The state stays DEPTH_WRITE for the texture's whole life. The heap's memory is not zeroed
        // (Allocator), which is fine: the renderer clears the depth before every use.
        m_allocation = m_allocator->CreateResource(D3D12_HEAP_TYPE_DEFAULT, desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                   &clearValue, m_name);

        // A DSV tells the output merger how to write to the texture; nullptr = its own format, mip 0
        m_device->CreateDepthStencilView(Native(), nullptr, m_dsv.Cpu());
    }
} // namespace engine::rhi