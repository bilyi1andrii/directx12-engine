#include "rhi/CommandContext.h"

#include <cstring>
#include <string>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    CommandContext::CommandContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name) {
        // CreateCommandList1 (ID3D12Device4) creates the list closed and without an allocator,
        // so it needs no Close() here; Begin() attaches the frame's allocator.
        ThrowIfFailed(device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_list)));
        ThrowIfFailed(m_list->SetName(std::wstring(name).c_str()));
    }

    void CommandContext::Begin(ID3D12CommandAllocator *allocator) {
        ENGINE_ASSERT_MSG(!m_recording, "CommandContext::Begin called while already recording");
        // Unlike the allocator, the list itself can be reset as soon as it has been submitted:
        // the recorded commands live in the allocator's memory, not in the list
        ThrowIfFailed(m_list->Reset(allocator, nullptr));
        m_recording = true;
    }

    void CommandContext::Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                                    UINT subresource) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT_MSG(before != after, "Transition with identical before/after state {}", static_cast<int>(before));

        if (m_pendingBarrierCount == kMaxPendingBarriers) {
            FlushBarriers();
        }
        // A transition barrier tells the GPU that the resource changes use (e.g. presented image ->
        // render target). The GPU may need to finish earlier work, flush caches or change the memory
        // layout (compression) before the next use, so every change of use needs one.
        m_pendingBarriers[m_pendingBarrierCount++] = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition =
                {
                    .pResource = resource,
                    .Subresource = subresource,
                    .StateBefore = before,
                    .StateAfter = after,
                },
        };
    }

    void CommandContext::FlushBarriers() {
        // One call with several barriers lets the driver combine their waits and cache flushes
        if (m_pendingBarrierCount > 0) {
            m_list->ResourceBarrier(m_pendingBarrierCount, m_pendingBarriers.data());
            m_pendingBarrierCount = 0;
        }
    }

    void CommandContext::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::span<const float, 4> color) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
    }

    void CommandContext::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    void CommandContext::CopyBuffer(ID3D12Resource *dst, uint64_t dstOffset, ID3D12Resource *src, uint64_t srcOffset,
                                    uint64_t size) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->CopyBufferRegion(dst, dstOffset, src, srcOffset, size);
    }

    void CommandContext::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION &dst,
                                           const D3D12_TEXTURE_COPY_LOCATION &src) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    void CommandContext::SetRenderTargets(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> rtvs,
                                          const D3D12_CPU_DESCRIPTOR_HANDLE *dsv) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT(rtvs.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
        // No DSV: no depth test or write
        m_list->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, dsv);
    }

    void CommandContext::SetViewportAndScissor(uint32_t width, uint32_t height) {
        ENGINE_ASSERT(m_recording);
        // Viewport: maps clip space to pixels and depth to [0, 1]. Scissor: pixels outside the rectangle
        // are discarded. Neither has a default in D3D12; without them nothing is drawn.
        const D3D12_VIEWPORT viewport{
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(width),
            .Height = static_cast<float>(height),
            .MinDepth = D3D12_MIN_DEPTH,
            .MaxDepth = D3D12_MAX_DEPTH,
        };
        const D3D12_RECT scissor{
            .left = 0, .top = 0, .right = static_cast<LONG>(width), .bottom = static_cast<LONG>(height)};
        m_list->RSSetViewports(1, &viewport);
        m_list->RSSetScissorRects(1, &scissor);
    }

    void CommandContext::SetDescriptorHeap(ID3D12DescriptorHeap *heap) {
        ENGINE_ASSERT(m_recording);
        // Shaders read ResourceDescriptorHeap[i] from the heap bound here. Only one CBV/SRV/UAV heap can be
        // bound at a time.
        m_list->SetDescriptorHeaps(1, &heap);
    }

    void CommandContext::SetGraphicsRootSignature(ID3D12RootSignature *rootSignature) {
        ENGINE_ASSERT(m_recording);
        m_list->SetGraphicsRootSignature(rootSignature);
    }

    void CommandContext::SetPipelineState(ID3D12PipelineState *pipelineState) {
        ENGINE_ASSERT(m_recording);
        m_list->SetPipelineState(pipelineState);
    }

    void CommandContext::SetGraphicsRootConstantBytes(uint32_t rootIndex, std::span<const std::byte> data) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT_MSG(data.size() % 4 == 0, "root constants must be 32-bit values, got {} bytes", data.size());
        // The values are copied into the command list now, so `data` may go away after this call
        m_list->SetGraphicsRoot32BitConstants(rootIndex, static_cast<UINT>(data.size() / 4), data.data(), 0);
    }

    void CommandContext::SetGraphicsRootConstantBufferView(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT_MSG(address % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
                          "constant buffer address {:#x} is not 256-byte aligned", address);
        m_list->SetGraphicsRootConstantBufferView(rootIndex, address);
    }

    void CommandContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology) {
        ENGINE_ASSERT(m_recording);
        m_list->IASetPrimitiveTopology(topology);
    }

    void CommandContext::SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW &view) {
        ENGINE_ASSERT(m_recording);
        m_list->IASetVertexBuffers(0, 1, &view);
    }

    void CommandContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW &view) {
        ENGINE_ASSERT(m_recording);
        m_list->IASetIndexBuffer(&view);
    }

    void CommandContext::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
    }

    void CommandContext::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex,
                                              int32_t baseVertex) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, 0);
    }

    void CommandContext::BeginEvent(const char *name) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT(name != nullptr);
        // Metadata 1 marks the data as an 8-bit string, the format PIX reads without its event runtime
        // (WINPIX_EVENT_ANSI_VERSION in pix3.h). The size includes the terminating null.
        constexpr UINT kAnsiString = 1;
        m_list->BeginEvent(kAnsiString, name, static_cast<UINT>(std::strlen(name) + 1));
    }

    void CommandContext::EndEvent() {
        ENGINE_ASSERT(m_recording);
        m_list->EndEvent();
    }

    void CommandContext::WriteTimestamp(ID3D12QueryHeap *heap, uint32_t index) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->EndQuery(heap, D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void CommandContext::ResolveTimestamps(ID3D12QueryHeap *heap, uint32_t first, uint32_t count,
                                           ID3D12Resource *destination, uint64_t offset) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT_MSG(offset % sizeof(uint64_t) == 0, "timestamp offset {} is not a multiple of 8", offset);
        FlushBarriers();
        m_list->ResolveQueryData(heap, D3D12_QUERY_TYPE_TIMESTAMP, first, count, destination, offset);
    }

    void CommandContext::Close() {
        ENGINE_ASSERT_MSG(m_recording, "CommandContext::Close called without Begin");
        FlushBarriers();
        ThrowIfFailed(m_list->Close());
        m_recording = false;
    }
} // namespace engine::rhi