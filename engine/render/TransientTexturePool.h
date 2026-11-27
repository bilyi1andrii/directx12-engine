#ifndef D3D12_ENGINE_TRANSIENTTEXTUREPOOL_H
#define D3D12_ENGINE_TRANSIENTTEXTUREPOOL_H

// The frame graph's textures that live only inside a frame, such as scene depth and the shadow map. They are kept
// between frames and rebuilds of the graph, so a texture is created only when no free one matches.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class BindlessHeap;
    class FrameContext;
} // namespace rhi

namespace render {
    struct TransientTextureDesc {
        enum class Target : uint8_t {
            Color,
            Depth,
        };

        uint32_t width = 0;
        uint32_t height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        Target target = Target::Color;
        bool shaderRead = false;

        friend bool operator==(const TransientTextureDesc &, const TransientTextureDesc &) = default;
    };

    class TransientTexturePool {
    public:
        struct Request {
            TransientTextureDesc desc;
            std::optional<std::array<float, 4>> clearColor;            // optimized clear value, part of match
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON; // graph's rest state
            std::string_view name;                                     // ASCII
        };
        struct Texture {
            ID3D12Resource *resource = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE view{};
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t srvIndex = kNoSrv; // bindless
            // before this build, graph fixes difference
            D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
        };
        static constexpr uint32_t kNoSrv = UINT32_MAX;

        TransientTexturePool(rhi::NativeDevice *device, rhi::Allocator &allocator,
                             rhi::DescriptorAllocator &rtvAllocator, rhi::DescriptorAllocator &dsvAllocator,
                             rhi::BindlessHeap &heap);
        ~TransientTexturePool();

        TransientTexturePool(const TransientTexturePool &) = delete;
        TransientTexturePool &operator=(const TransientTexturePool &) = delete;
        TransientTexturePool(TransientTexturePool &&) = delete;
        TransientTexturePool &operator=(TransientTexturePool &&) = delete;

        void Acquire(std::span<const Request> requests, rhi::FrameContext &frame, std::vector<Texture> &textures);

        void DestroyAll(rhi::FrameContext &frame) noexcept;

    private:
        struct Entry {
            TransientTextureDesc desc;
            std::optional<std::array<float, 4>> clearColor;
            rhi::Allocation allocation;
            rhi::Descriptor view;
            uint32_t srvIndex = kNoSrv;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
        };

        [[nodiscard]] Entry Create(const Request &request);
        void Destroy(Entry &entry, rhi::FrameContext &frame) noexcept;

        rhi::NativeDevice *m_device;
        rhi::Allocator &m_allocator;
        rhi::DescriptorAllocator &m_rtvAllocator;
        rhi::DescriptorAllocator &m_dsvAllocator;
        rhi::BindlessHeap &m_heap;
        std::vector<Entry> m_entries;
        std::vector<bool> m_acquired; // taken in current Acquire
    };
} // namespace render

#endif // D3D12_ENGINE_TRANSIENTTEXTUREPOOL_H
