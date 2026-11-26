#include "render/TransientTexturePool.h"

#include <algorithm>
#include <string>
#include <utility>

#include "core/Assert.h"
#include "rhi/BindlessHeap.h"
#include "rhi/DepthFormat.h"
#include "rhi/FrameContext.h"

namespace render {
    TransientTexturePool::TransientTexturePool(rhi::NativeDevice *device, rhi::Allocator &allocator,
                                               rhi::DescriptorAllocator &rtvAllocator,
                                               rhi::DescriptorAllocator &dsvAllocator, rhi::BindlessHeap &heap)
        : m_device(device), m_allocator(allocator), m_rtvAllocator(rtvAllocator), m_dsvAllocator(dsvAllocator),
          m_heap(heap) {}

    TransientTexturePool::~TransientTexturePool() {
        ENGINE_ASSERT_MSG(m_entries.empty(), "TransientTexturePool destroyed without DestroyAll");
    }

    void TransientTexturePool::Acquire(std::span<const Request> requests, rhi::FrameContext &frame,
                                       std::vector<Texture> &textures) {
        textures.clear();
        m_acquired.assign(m_entries.size(), false);
        for (const Request &request : requests) {
            size_t index = 0;
            while (index < m_entries.size() && (m_acquired[index] || m_entries[index].desc != request.desc ||
                                                m_entries[index].clearColor != request.clearColor)) {
                ++index;
            }
            if (index == m_entries.size()) {
                m_entries.push_back(Create(request));
                m_acquired.push_back(false);
            }
            m_acquired[index] = true;

            Entry &entry = m_entries[index];
            textures.push_back({.resource = entry.allocation->GetResource(),
                                .view = entry.view.Cpu(),
                                .width = entry.desc.width,
                                .height = entry.desc.height,
                                .srvIndex = entry.srvIndex,
                                .currentState = entry.state});
            entry.state = request.state;
        }

        size_t kept = 0;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_acquired[i]) {
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

        static_assert(rhi::kDepthFormat == DXGI_FORMAT_D32_FLOAT,
                      "The typeless and shader-view formats below are those of D32_FLOAT: change them with the format");
        const DXGI_FORMAT resourceFormat = depth && desc.shaderRead ? DXGI_FORMAT_R32_TYPELESS : desc.format;
        const DXGI_FORMAT srvFormat = depth ? DXGI_FORMAT_R32_FLOAT : desc.format;

        D3D12_RESOURCE_FLAGS flags =
            depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (depth && !desc.shaderRead) {
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        const D3D12_RESOURCE_DESC resourceDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = desc.width,
            .Height = desc.height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = resourceFormat,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = flags,
        };

        D3D12_CLEAR_VALUE clearValue{.Format = depth ? rhi::kDepthFormat : desc.format, .Color = {}};
        if (depth) {
            clearValue.DepthStencil = {.Depth = rhi::kClearDepth, .Stencil = 0};
        } else if (request.clearColor) {
            std::ranges::copy(*request.clearColor, clearValue.Color);
        }
        const bool hasClearValue = depth || request.clearColor.has_value();

        const std::string name = std::string("FG/") + std::string(request.name);
        Entry entry{
            .desc = desc,
            .clearColor = request.clearColor,
            .allocation = m_allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, resourceDesc, request.state,
                                                     hasClearValue ? &clearValue : nullptr,
                                                     std::wstring(name.begin(), name.end())),
            .view = depth ? m_dsvAllocator.Allocate() : m_rtvAllocator.Allocate(),
            .srvIndex = kNoSrv,
            .state = request.state,
        };
        ID3D12Resource *resource = entry.allocation->GetResource();

        if (depth) {
            const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
                .Format = rhi::kDepthFormat,
                .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                .Flags = D3D12_DSV_FLAG_NONE,
                .Texture2D = {.MipSlice = 0},
            };
            m_device->CreateDepthStencilView(resource, &dsvDesc, entry.view.Cpu());
        } else {
            m_device->CreateRenderTargetView(resource, nullptr, entry.view.Cpu());
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
        entry.view.Reset();
    }
} // namespace render
