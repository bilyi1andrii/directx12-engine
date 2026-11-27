// First: UiRenderer.h brings the Agility SDK's d3d12.h, so imgui_impl_dx12.h's Windows SDK copy is skipped
#include "render/UiRenderer.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_dx12.h>

#include "rhi/SwapChain.h"

namespace render {
    UiRenderer::UiRenderer(ID3D12Device *device, ID3D12CommandQueue *queue, rhi::BindlessHeap &heap,
                           rhi::FrameContext &frame)
        : m_heap(heap), m_frame(&frame) {
        ImGui_ImplDX12_InitInfo info;
        info.Device = device;
        info.CommandQueue = queue;
        info.NumFramesInFlight = static_cast<int>(rhi::kFramesInFlight);
        info.RTVFormat = rhi::SwapChain::kFormat;
        info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        info.UserData = this;
        info.SrvDescriptorHeap = heap.Native(); // ImGui's font descriptor from bindless heap
        info.SrvDescriptorAllocFn = &UiRenderer::AllocateDescriptor;
        info.SrvDescriptorFreeFn = &UiRenderer::FreeDescriptor;
        if (!ImGui_ImplDX12_Init(&info)) {
            throw std::runtime_error("ImGui: the DirectX 12 backend failed to initialize");
        }
        if (!ImGui_ImplDX12_CreateDeviceObjects()) {
            ImGui_ImplDX12_Shutdown();
            throw std::runtime_error("ImGui: the DirectX 12 backend failed to create its root signature or PSO");
        }
    }

    UiRenderer::~UiRenderer() {
        ImGui_ImplDX12_Shutdown();
    }

    void UiRenderer::Render(ImDrawData *drawData, rhi::CommandContext &context, rhi::FrameContext &frame) {
        m_frame = &frame;
        ImGui_ImplDX12_RenderDrawData(drawData, context.Native());
    }

    void UiRenderer::AllocateDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu,
                                        D3D12_GPU_DESCRIPTOR_HANDLE *gpu) {
        auto *self = static_cast<UiRenderer *>(info->UserData);
        const uint32_t index = self->m_heap.Allocate();
        *cpu = self->m_heap.CpuHandle(index);
        *gpu = self->m_heap.GpuHandle(index);
    }

    void UiRenderer::FreeDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                    D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/) {
        auto *self = static_cast<UiRenderer *>(info->UserData);
        self->m_frame->DeferFree(self->m_heap, self->m_heap.IndexOf(cpu));
    }
} // namespace render
