#ifndef D3D12_ENGINE_COMMANDCONTEXT_H
#define D3D12_ENGINE_COMMANDCONTEXT_H

// Records GPU commands into one command list. Resource barriers are collected and recorded together, just before
// the next command that needs them.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "rhi/GpuCopyable.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class CommandContext {
    public:
        CommandContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name);

        CommandContext(const CommandContext &) = delete;
        CommandContext &operator=(const CommandContext &) = delete;
        CommandContext(CommandContext &&) = delete;
        CommandContext &operator=(CommandContext &&) = delete;

        void Begin(ID3D12CommandAllocator *allocator);

        void Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        void FlushBarriers();
        [[nodiscard]] bool HasPendingBarriers() const noexcept { return m_pendingBarrierCount != 0; }

        void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::span<const float, 4> color);

        void ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth);

        void CopyBuffer(ID3D12Resource *dst, uint64_t dstOffset, ID3D12Resource *src, uint64_t srcOffset,
                        uint64_t size);
        void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION &dst, const D3D12_TEXTURE_COPY_LOCATION &src);

        void SetRenderTargets(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> rtvs,
                              const D3D12_CPU_DESCRIPTOR_HANDLE *dsv);
        void SetViewportAndScissor(uint32_t width, uint32_t height);
        void SetDescriptorHeap(ID3D12DescriptorHeap *heap);
        void SetGraphicsRootSignature(ID3D12RootSignature *rootSignature);
        void SetPipelineState(ID3D12PipelineState *pipelineState);
        template <GpuCopyable T> void SetGraphicsRootConstants(uint32_t rootIndex, const T &constants) {
            static_assert(sizeof(T) % 4 == 0, "root constants are 32-bit values");
            SetGraphicsRootConstantBytes(rootIndex, std::as_bytes(std::span(&constants, 1)));
        }
        void SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value, uint32_t offset) {
            m_list->SetGraphicsRoot32BitConstant(rootIndex, value, offset);
        }
        void SetGraphicsRootConstantBufferView(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
        void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW &view);
        void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW &view);

        void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0);
        void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0,
                                  int32_t baseVertex = 0);

        void BeginEvent(const char *name);
        void EndEvent();

        void WriteTimestamp(ID3D12QueryHeap *heap, uint32_t index);
        void ResolveTimestamps(ID3D12QueryHeap *heap, uint32_t first, uint32_t count, ID3D12Resource *destination,
                               uint64_t offset);

        void Close();

        [[nodiscard]] ID3D12GraphicsCommandList *Native() const noexcept { return m_list.Get(); }

    private:
        static constexpr uint32_t kMaxPendingBarriers = 16;

        void SetGraphicsRootConstantBytes(uint32_t rootIndex, std::span<const std::byte> data);

        ComPtr<ID3D12GraphicsCommandList> m_list;
        std::array<D3D12_RESOURCE_BARRIER, kMaxPendingBarriers> m_pendingBarriers{};
        uint32_t m_pendingBarrierCount = 0;
    };

    class ScopedGpuEvent {
    public:
        ScopedGpuEvent(CommandContext &context, const char *name) : m_context(context) { m_context.BeginEvent(name); }
        ~ScopedGpuEvent() { m_context.EndEvent(); }

        ScopedGpuEvent(const ScopedGpuEvent &) = delete;
        ScopedGpuEvent &operator=(const ScopedGpuEvent &) = delete;
        ScopedGpuEvent(ScopedGpuEvent &&) = delete;
        ScopedGpuEvent &operator=(ScopedGpuEvent &&) = delete;

    private:
        CommandContext &m_context;
    };
} // namespace rhi

#endif // D3D12_ENGINE_COMMANDCONTEXT_H
