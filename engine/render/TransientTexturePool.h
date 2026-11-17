#ifndef D3D12_ENGINE_TRANSIENTTEXTUREPOOL_H
#define D3D12_ENGINE_TRANSIENTTEXTUREPOOL_H

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class BindlessHeap;
    class FrameContext;
} // namespace engine::rhi

namespace engine::render {
    // What a frame graph's transient texture is. Textures with equal descriptions are interchangeable.
    struct TransientTextureDesc {
        enum class Target : uint8_t {
            Color, // drawn into through a render-target view (RTV)
            Depth, // depth-tested through a depth-stencil view (DSV); only rhi::kDepthFormat
        };

        uint32_t width = 0;
        uint32_t height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; // what the views read and write
        Target target = Target::Color;
        bool shaderRead = false; // also sampled by shaders, through a bindless view

        friend bool operator==(const TransientTextureDesc &, const TransientTextureDesc &) = default;
    };

    // The textures of a frame graph that live only inside a frame (scene depth, shadow map), kept between frames
    // and between rebuilds of the graph, so a texture is created only when no free one matches.
    //
    // Resource states persist between command lists on one queue, so the pool remembers the state each texture
    // is in. Call DestroyAll before destroying it (asserted). The allocators and the bindless heap must outlive it.
    class TransientTexturePool {
    public:
        struct Request {
            TransientTextureDesc desc;
            // The state the texture must be in when the next frame starts
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
            std::string_view name; // ASCII; for a new texture's debug name
        };
        struct Texture {
            ID3D12Resource *resource = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE view{}; // RTV or DSV
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t srvIndex = kNoSrv; // bindless index, if the desc asks for shader reads
            // The state the texture is in now. If it differs from the request's state, the caller must change it
            // before the texture's first use.
            D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
        };
        static constexpr uint32_t kNoSrv = UINT32_MAX;

        TransientTexturePool(rhi::NativeDevice *device, rhi::Allocator &allocator, rhi::DescriptorAllocator &rtvAllocator,
                             rhi::DescriptorAllocator &dsvAllocator, rhi::BindlessHeap &heap);
        ~TransientTexturePool();

        TransientTexturePool(const TransientTexturePool &) = delete;
        TransientTexturePool &operator=(const TransientTexturePool &) = delete;
        TransientTexturePool(TransientTexturePool &&) = delete;
        TransientTexturePool &operator=(TransientTexturePool &&) = delete;

        // Gives one texture per request (into `textures`, in order): a pool texture of the same description, or a
        // new one created in the request's state. Each texture goes to at most one request. Textures no request
        // took are destroyed once the GPU has finished `frame`. Afterwards the pool counts each texture as being in
        // its request's state: the caller makes that true before the texture's first use.
        void Take(std::span<const Request> requests, rhi::FrameContext &frame, std::vector<Texture> &textures);

        // Destroys every texture once the GPU has finished `frame`. Call before destruction.
        void DestroyAll(rhi::FrameContext &frame) noexcept;

    private:
        struct Entry {
            TransientTextureDesc desc;
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
        std::vector<bool> m_taken; // scratch for Take
    };
} // namespace engine::render

#endif // D3D12_ENGINE_TRANSIENTTEXTUREPOOL_H
