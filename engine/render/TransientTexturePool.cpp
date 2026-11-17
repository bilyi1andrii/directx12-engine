#include "render/TransientTexturePool.h"

#include <string>
#include <utility>

#include "core/Assert.h"
#include "rhi/BindlessHeap.h"
#include "rhi/DepthFormat.h"
#include "rhi/FrameContext.h"

namespace engine::render {
    TransientTexturePool::TransientTexturePool(rhi::NativeDevice *device, rhi::Allocator &allocator,
                                               rhi::DescriptorAllocator &rtvAllocator,
                                               rhi::DescriptorAllocator &dsvAllocator, rhi::BindlessHeap &heap)
        : m_device(device), m_allocator(allocator), m_rtvAllocator(rtvAllocator), m_dsvAllocator(dsvAllocator),
          m_heap(heap) {}

    TransientTexturePool::~TransientTexturePool() {
        ENGINE_ASSERT_MSG(m_entries.empty(), "TransientTexturePool destroyed without DestroyAll");
    }

    void TransientTexturePool::Take(std::span<const Request> requests, rhi::FrameContext &frame,
                                    std::vector<Texture> &textures) {
        textures.clear();
        m_taken.assign(m_entries.size(), false);
        for (const Request &request : requests) {
            size_t index = 0;
            while (index < m_entries.size() && (m_taken[index] || m_entries[index].desc != request.desc)) {
                ++index;
            }
            if (index == m_entries.size()) {
                m_entries.push_back(Create(request));
                m_taken.push_back(false);
            }
            m_taken[index] = true;

            Entry &entry = m_entries[index];
            textures.push_back({.resource = entry.allocation->GetResource(),
                                .view = entry.view.Cpu(),
                                .width = entry.desc.width,
                                .height = entry.desc.height,
                                .srvIndex = entry.srvIndex,
                                .currentState = entry.state});
            entry.state = request.state;
        }

        // Textures the new graph does not use. Frames still in flight may use them, so they go only after `frame`.
        size_t kept = 0;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_taken[i]) {
                if (kept != i) {
                    m_entries[kept] = std::move(m_entries[i]);
                }
                ++kept;
            } else {
                Destroy(m_entries[i], frame);
            }
        }
        m_entries.resize(kept);
    }

    void TransientTexturePool::DestroyAll(rhi::FrameContext &frame) noexcept {
        for (Entry &entry : m_entries) {
            Destroy(entry, frame);
        }
        m_entries.clear();
    }

    TransientTexturePool::Entry TransientTexturePool::Create(const Request &request) {
        const TransientTextureDesc &desc = request.desc;
        const bool depth = desc.target == TransientTextureDesc::Target::Depth;
        ENGINE_ASSERT_MSG(!depth || desc.format == rhi::kDepthFormat, "Transient depth texture {} is not kDepthFormat",
                          request.name);
        ENGINE_ASSERT_MSG(desc.width > 0 && desc.height > 0, "Transient texture {} has no size", request.name);

        // A depth texture that shaders also sample is created without a type (TYPELESS): its depth view reads it
        // as depth, its shader view as a plain float
        const DXGI_FORMAT resourceFormat = depth && desc.shaderRead ? DXGI_FORMAT_R32_TYPELESS : desc.format;
        const DXGI_FORMAT srvFormat = depth ? DXGI_FORMAT_R32_FLOAT : desc.format;

        D3D12_RESOURCE_FLAGS flags =
            depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (depth && !desc.shaderRead) {
            // Lets the driver keep depth in a layout only the depth test understands
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        const D3D12_RESOURCE_DESC resourceDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0, // default (64 KB)
            .Width = desc.width,
            .Height = desc.height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = resourceFormat,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, // the driver picks the (tiled) memory layout
            .Flags = flags,
        };

        // Optimized clear value: GPUs keep compressed depth data and can clear it almost for free, but only to the
        // value given at creation. Clearing to any other value works, but is slower and the debug layer warns.
        // Colour targets get none, because the passes choose their clear colour.
        const D3D12_CLEAR_VALUE depthClear{
            .Format = rhi::kDepthFormat,
            .DepthStencil = {.Depth = rhi::kClearDepth, .Stencil = 0},
        };

        // Default heap: GPU-local memory the CPU cannot access, where render targets and depth live. Not zeroed:
        // the passes clear what they draw into.
        const std::string name = std::string("FG/") + std::string(request.name);
        Entry entry{
            .desc = desc,
            .allocation =
                m_allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, resourceDesc, request.state,
                                           depth ? &depthClear : nullptr, std::wstring(name.begin(), name.end())),
            .view = depth ? m_dsvAllocator.Allocate() : m_rtvAllocator.Allocate(),
            .srvIndex = kNoSrv,
            .state = request.state,
        };
        ID3D12Resource *resource = entry.allocation->GetResource();

        if (depth) {
            // A depth-stencil view (DSV) tells the output merger how to depth-test against the texture
            const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
                .Format = rhi::kDepthFormat,
                .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                .Flags = D3D12_DSV_FLAG_NONE,
                .Texture2D = {.MipSlice = 0},
            };
            m_device->CreateDepthStencilView(resource, &dsvDesc, entry.view.Cpu());
        } else {
            m_device->CreateRenderTargetView(resource, nullptr, entry.view.Cpu()); // nullptr: its own format, mip 0
        }
        if (desc.shaderRead) {
            entry.srvIndex = m_heap.Allocate();
            m_heap.CreateTexture2DSrv(entry.srvIndex, resource, srvFormat, 1);
        }
        return entry;
    }

    void TransientTexturePool::Destroy(Entry &entry, rhi::FrameContext &frame) noexcept {
        frame.DeferRelease(std::move(entry.allocation));
        if (entry.srvIndex != kNoSrv) {
            frame.DeferFree(m_heap, entry.srvIndex);
            entry.srvIndex = kNoSrv;
        }
        // CPU-only descriptors are copied into the command list when recorded, so the slot can go back at once
        entry.view.Reset();
    }
} // namespace engine::render
