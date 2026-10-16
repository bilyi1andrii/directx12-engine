#include "rhi/DepthBuffer.h"

#include <utility>

#include "core/ThrowIfFailed.h"
#include "rhi/FrameContext.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    DepthBuffer::DepthBuffer(NativeDevice *device, DescriptorAllocator &dsvAllocator, uint32_t width, uint32_t height,
                             std::wstring_view name)
        : m_device(device), m_dsv(dsvAllocator.Allocate()), m_width(width), m_height(height), m_name(name) {
        Create();
    }

    void DepthBuffer::Resize(uint32_t width, uint32_t height, FrameContext &frame) {
        if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
            return;
        }

        // Frames still in flight may be testing against the old texture. Overwriting the DSV slot is
        // fine: CPU-only descriptors are copied into the command list when a command is recorded.
        frame.DeferRelease(std::move(m_resource));
        m_width = width;
        m_height = height;
        Create();
    }

    void DepthBuffer::Create() {
        // Default heap: GPU-local memory the CPU cannot access, where render targets and depth live
        const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};

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

        // Committed resource: its own heap, sized for this texture. Until D3D12MA arrives in step 2
        // (see PROGRESS.md); the state stays DEPTH_WRITE for the texture's whole life.
        ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                                        IID_PPV_ARGS(&m_resource)));
        ThrowIfFailed(m_resource->SetName(m_name.c_str()));

        // A DSV tells the output merger how to write to the texture; nullptr = its own format, mip 0
        m_device->CreateDepthStencilView(m_resource.Get(), nullptr, m_dsv.Cpu());
    }
} // namespace engine::rhi