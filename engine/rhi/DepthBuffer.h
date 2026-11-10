#ifndef D3D12_ENGINE_DEPTHBUFFER_H
#define D3D12_ENGINE_DEPTHBUFFER_H

#include <cstdint>
#include <string>
#include <string_view>

#include "rhi/Allocator.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class FrameContext;

    // A depth texture the size of the render target, plus its depth-stencil view (DSV).
    // It always stays in DEPTH_WRITE, so it needs no barriers while nothing samples it.
    // Created through rhi::Allocator.
    //
    // Destroying it releases the texture and its memory at once: the owner must make sure the GPU is idle
    // (Renderer flushes the queue first). Resize() defers the old texture instead.
    //
    // Can be move-constructed but not assigned: assigning would release the old texture at once instead of deferring
    // it. The resource allocator and the DSV allocator must outlive it.
    class DepthBuffer {
    public:
        // 32-bit float depth, no stencil
        static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_D32_FLOAT;
        // Far plane with the standard (not reversed) depth range; the depth test is LESS
        static constexpr float kClearDepth = 1.0f;

        DepthBuffer(NativeDevice *device, Allocator &allocator, DescriptorAllocator &dsvAllocator, uint32_t width,
                    uint32_t height, std::wstring_view name);

        DepthBuffer(const DepthBuffer &) = delete;
        DepthBuffer &operator=(const DepthBuffer &) = delete;
        DepthBuffer(DepthBuffer &&) noexcept = default;
        DepthBuffer &operator=(DepthBuffer &&) = delete;

        // Recreates the texture at the new size. The old texture goes to `frame`'s deferred-release
        // list, because earlier frames may still be using it. Does nothing for an unchanged size or 0x0.
        void Resize(uint32_t width, uint32_t height, FrameContext &frame);

        [[nodiscard]] ID3D12Resource *Native() const noexcept { return m_allocation->GetResource(); }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const noexcept { return m_dsv.Cpu(); }
        [[nodiscard]] uint32_t Width() const noexcept { return m_width; }
        [[nodiscard]] uint32_t Height() const noexcept { return m_height; }

    private:
        void Create();

        NativeDevice *m_device;
        Allocator *m_allocator;
        Allocation m_allocation;
        Descriptor m_dsv; // the slot is kept across resizes; only the view in it is rewritten
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        std::wstring m_name;
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_DEPTHBUFFER_H