#include "rhi/CommandContext.h"

#include <cstring>
#include <string>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"

namespace rhi {
    using core::ThrowIfFailed;

    CommandContext::CommandContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name) {
        ThrowIfFailed(device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_list)));
        ThrowIfFailed(m_list->SetName(std::wstring(name).c_str()));
    }

    void CommandContext::Begin(ID3D12CommandAllocator *allocator) {
        ThrowIfFailed(m_list->Reset(allocator, nullptr));
    }

    void CommandContext::Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                                    UINT subresource) {
        if (m_pendingBarrierCount == kMaxPendingBarriers) {
            FlushBarriers();
        }
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
        if (m_pendingBarrierCount > 0) {
            m_list->ResourceBarrier(m_pendingBarrierCount, m_pendingBarriers.data());
            m_pendingBarrierCount = 0;
        }
    }

    void CommandContext::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::span<const float, 4> color) {
        FlushBarriers();
        m_list->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
    }

    void CommandContext::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth) {
        FlushBarriers();
        m_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    void CommandContext::CopyBuffer(ID3D12Resource *dst, uint64_t dstOffset, ID3D12Resource *src, uint64_t srcOffset,
                                    uint64_t size) {
        FlushBarriers();
        m_list->CopyBufferRegion(dst, dstOffset, src, srcOffset, size);
    }

    void CommandContext::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION &dst,
                                           const D3D12_TEXTURE_COPY_LOCATION &src) {
        FlushBarriers();
        m_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    void CommandContext::SetRenderTargets(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> rtvs,
                                          const D3D12_CPU_DESCRIPTOR_HANDLE *dsv) {
        m_list->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, dsv);
    }

    void CommandContext::SetViewportAndScissor(uint32_t width, uint32_t height) {
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
        m_list->SetDescriptorHeaps(1, &heap);
    }

    void CommandContext::SetGraphicsRootSignature(ID3D12RootSignature *rootSignature) {
        m_list->SetGraphicsRootSignature(rootSignature);
    }

    void CommandContext::SetPipelineState(ID3D12PipelineState *pipelineState) {
        m_list->SetPipelineState(pipelineState);
    }

    void CommandContext::SetGraphicsRootConstantBytes(uint32_t rootIndex, std::span<const std::byte> data) {
        m_list->SetGraphicsRoot32BitConstants(rootIndex, static_cast<UINT>(data.size() / 4), data.data(), 0);
    }

    void CommandContext::SetGraphicsRootConstantBufferView(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address) {
        ENGINE_ASSERT_MSG(address % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
                          "constant buffer address {:#x} is not 256-byte aligned", address);
        m_list->SetGraphicsRootConstantBufferView(rootIndex, address);
    }

    void CommandContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology) {
        m_list->IASetPrimitiveTopology(topology);
    }

    void CommandContext::SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW &view) {
        m_list->IASetVertexBuffers(0, 1, &view);
    }

    void CommandContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW &view) {
        m_list->IASetIndexBuffer(&view);
    }

    void CommandContext::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) {
        FlushBarriers();
        m_list->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
    }

    void CommandContext::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex,
                                              int32_t baseVertex) {
        FlushBarriers();
        m_list->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, 0);
    }

    void CommandContext::BeginEvent(const char *name) {
        constexpr UINT kAnsiString = 1;
        m_list->BeginEvent(kAnsiString, name, static_cast<UINT>(std::strlen(name) + 1));
    }

    void CommandContext::EndEvent() {
        m_list->EndEvent();
    }

    void CommandContext::WriteTimestamp(ID3D12QueryHeap *heap, uint32_t index) {
        FlushBarriers();
        m_list->EndQuery(heap, D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void CommandContext::ResolveTimestamps(ID3D12QueryHeap *heap, uint32_t first, uint32_t count,
                                           ID3D12Resource *destination, uint64_t offset) {
        FlushBarriers();
        m_list->ResolveQueryData(heap, D3D12_QUERY_TYPE_TIMESTAMP, first, count, destination, offset);
    }

    void CommandContext::Close() {
        FlushBarriers();
        ThrowIfFailed(m_list->Close());
    }
} // namespace rhi
