#ifndef D3D12_ENGINE_COMMANDCONTEXT_H
#define D3D12_ENGINE_COMMANDCONTEXT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/Assert.h"
#include "rhi/GpuCopyable.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // Thin wrapper over a graphics command list. Collects resource barriers and submits them as one
    // ResourceBarrier() call right before the next command that needs them (or at Close()).
    //
    // Usage per frame: Begin(frame.CommandAllocator()) -> Transition/Clear/... -> Close() -> CommandQueue::ExecuteCommandList(Native()).
    // Calls made directly on Native() bypass the batching: call FlushBarriers() before them.
    //
    // Not copyable or movable.
    class CommandContext {
    public:
        CommandContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name);

        CommandContext(const CommandContext &) = delete;
        CommandContext &operator=(const CommandContext &) = delete;
        CommandContext(CommandContext &&) = delete;
        CommandContext &operator=(CommandContext &&) = delete;

        // Starts recording into memory from `allocator`. The allocator must not be in use by the GPU.
        void Begin(ID3D12CommandAllocator *allocator);

        // Queues a state transition. `before` must be the resource's current state.
        void Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        // Records all queued barriers
        void FlushBarriers();
        // True while barriers are queued but not recorded yet
        [[nodiscard]] bool HasPendingBarriers() const noexcept { return m_pendingBarrierCount != 0; }

        void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::span<const float, 4> color);

        // Clears depth only (no stencil). Use the depth buffer's optimized clear value
        void ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth);

        // Copies `size` bytes between two buffers. dst must be in (or promotable to) COPY_DEST,
        // src in COPY_SOURCE or GENERIC_READ (upload heap)
        void CopyBuffer(ID3D12Resource *dst, uint64_t dstOffset, ID3D12Resource *src, uint64_t srcOffset,
                        uint64_t size);
        // Copies a whole texture mip, e.g. from a staging buffer into a texture in COPY_DEST
        void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION &dst, const D3D12_TEXTURE_COPY_LOCATION &src);

        // Pipeline setup. These record state only; barriers are flushed at the next draw or clear.
        // Up to 8 colour targets and an optional depth target. Without a depth target (`dsv` null), the PSO's
        // DSVFormat must be DXGI_FORMAT_UNKNOWN.
        void SetRenderTargets(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> rtvs,
                              const D3D12_CPU_DESCRIPTOR_HANDLE *dsv);
        // Viewport and scissor covering the whole width x height target
        void SetViewportAndScissor(uint32_t width, uint32_t height);
        // Binds the shader-visible descriptor heap (BindlessHeap).
        void SetDescriptorHeap(ID3D12DescriptorHeap *heap);
        void SetGraphicsRootSignature(ID3D12RootSignature *rootSignature);
        void SetPipelineState(ID3D12PipelineState *pipelineState);
        // Writes a struct of 32-bit values into the root constants of parameter `rootIndex`.
        // The root signature must be set first.
        template <GpuCopyable T> void SetGraphicsRootConstants(uint32_t rootIndex, const T &constants) {
            static_assert(sizeof(T) % 4 == 0, "root constants are 32-bit values");
            SetGraphicsRootConstantBytes(rootIndex, std::as_bytes(std::span(&constants, 1)));
        }
        // Writes one 32-bit value at 32-bit `offset` into the root constants of parameter `rootIndex`; the
        // others keep their values. Defined here so the per-draw loop of the non-instanced path can inline it.
        void SetGraphicsRootConstant(uint32_t rootIndex, uint32_t value, uint32_t offset) {
            ENGINE_ASSERT(m_recording);
            m_list->SetGraphicsRoot32BitConstant(rootIndex, value, offset);
        }
        // Points root CBV parameter `rootIndex` at a constant buffer (256-byte aligned, from an
        // UploadRing). Only the address is recorded: the data must stay valid until the GPU has read it.
        void SetGraphicsRootConstantBufferView(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
        void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW &view);
        void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW &view);

        // One non-instanced indexed draw
        void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0);
        // `instanceCount` copies of the same indexed geometry; SV_InstanceID counts 0..instanceCount-1.
        // No start-instance parameter: in D3D12 it does not offset SV_InstanceID, so shaders get their
        // first instance from root constants instead.
        void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0,
                                  int32_t baseVertex = 0);

        // Starts and ends a named region of commands. GPU debuggers (PIX, RenderDoc, Nsight) show the regions as a
        // tree, so a capture reads as passes instead of one long list of draws. `name` must be null-terminated.
        void BeginEvent(std::string_view name);
        void EndEvent();

        // Flushes barriers and ends recording; the list can then be executed
        void Close();

        [[nodiscard]] ID3D12GraphicsCommandList *Native() const noexcept { return m_list.Get(); }

    private:
        static constexpr uint32_t kMaxPendingBarriers = 16;

        void SetGraphicsRootConstantBytes(uint32_t rootIndex, std::span<const std::byte> data);

        ComPtr<ID3D12GraphicsCommandList> m_list;
        std::array<D3D12_RESOURCE_BARRIER, kMaxPendingBarriers> m_pendingBarriers{};
        uint32_t m_pendingBarrierCount = 0;
        bool m_recording = false;
    };

    // A named region of commands from construction to destruction (CommandContext::BeginEvent / EndEvent)
    class ScopedGpuEvent {
    public:
        ScopedGpuEvent(CommandContext &context, std::string_view name) : m_context(context) {
            m_context.BeginEvent(name);
        }
        ~ScopedGpuEvent() { m_context.EndEvent(); }

        ScopedGpuEvent(const ScopedGpuEvent &) = delete;
        ScopedGpuEvent &operator=(const ScopedGpuEvent &) = delete;
        ScopedGpuEvent(ScopedGpuEvent &&) = delete;
        ScopedGpuEvent &operator=(ScopedGpuEvent &&) = delete;

    private:
        CommandContext &m_context;
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_COMMANDCONTEXT_H