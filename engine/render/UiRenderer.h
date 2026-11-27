#ifndef D3D12_ENGINE_UIRENDERER_H
#define D3D12_ENGINE_UIRENDERER_H

// Draws the debug UI with ImGui's own DirectX 12 backend. The backend's descriptors come from the bindless heap,
// like every other view the shaders read.

#include "rhi/BindlessHeap.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/RhiCommon.h"

struct ImDrawData;
struct ImGui_ImplDX12_InitInfo;

namespace render {
    class UiRenderer {
    public:
        UiRenderer(ID3D12Device *device, ID3D12CommandQueue *queue, rhi::BindlessHeap &heap, rhi::FrameContext &frame);
        ~UiRenderer();

        UiRenderer(const UiRenderer &) = delete;
        UiRenderer &operator=(const UiRenderer &) = delete;
        UiRenderer(UiRenderer &&) = delete;
        UiRenderer &operator=(UiRenderer &&) = delete;

        void Render(ImDrawData *drawData, rhi::CommandContext &context, rhi::FrameContext &frame);

    private:
        static void AllocateDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu,
                                       D3D12_GPU_DESCRIPTOR_HANDLE *gpu);
        static void FreeDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                   D3D12_GPU_DESCRIPTOR_HANDLE gpu);

        rhi::BindlessHeap &m_heap;
        rhi::FrameContext *m_frame;
    };
} // namespace render

#endif // D3D12_ENGINE_UIRENDERER_H
