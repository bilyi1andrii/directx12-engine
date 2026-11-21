// UiRenderer.h first: it brings the Agility SDK's <directx/d3d12.h>. imgui_impl_dx12.h includes the Windows SDK's
// <d3d12.h>, which has the same include guard and is then skipped, so this file sees one set of D3D12 declarations.
#include "render/UiRenderer.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_dx12.h>

#include "core/Assert.h"
#include "rhi/SwapChain.h"

namespace engine::render {
    UiRenderer::UiRenderer(ID3D12Device *device, ID3D12CommandQueue *queue, rhi::BindlessHeap &heap) : m_heap(heap) {
        ENGINE_ASSERT_MSG(ImGui::GetCurrentContext() != nullptr, "UiRenderer needs an ImGui context (ui::DebugUI)");

        ImGui_ImplDX12_InitInfo info;
        info.Device = device;
        // The backend uploads its font texture with its own command list on this queue, then waits for it
        info.CommandQueue = queue;
        // It keeps one vertex/index buffer per frame in flight and reuses one only kFramesInFlight calls later
        info.NumFramesInFlight = static_cast<int>(rhi::kFramesInFlight);
        // The UNORM view of the back buffer (SwapChain::CurrentUiRtv): ImGui's colours are already sRGB
        info.RTVFormat = rhi::SwapChain::kFormat;
        info.DSVFormat = DXGI_FORMAT_UNKNOWN; // drawn on top of everything, no depth buffer
        info.UserData = this;
        info.SrvDescriptorHeap = heap.Native();
        info.SrvDescriptorAllocFn = &UiRenderer::AllocateDescriptor;
        info.SrvDescriptorFreeFn = &UiRenderer::FreeDescriptor;
        if (!ImGui_ImplDX12_Init(&info)) {
            throw std::runtime_error("ImGui: the DirectX 12 backend failed to initialize");
        }
        // Root signature and PSO now rather than lazily, so a failure is reported here (an exception, not an
        // IM_ASSERT in the middle of a frame)
        if (!ImGui_ImplDX12_CreateDeviceObjects()) {
            ImGui_ImplDX12_Shutdown();
            throw std::runtime_error("ImGui: the DirectX 12 backend failed to create its root signature or PSO");
        }
    }

    UiRenderer::~UiRenderer() {
        // Releases the PSO, buffers and textures, and frees the textures' descriptors (FreeDescriptor)
        ImGui_ImplDX12_Shutdown();
    }

    void UiRenderer::Render(ImDrawData *drawData, rhi::CommandContext &context, rhi::FrameContext &frame) {
        m_frame = &frame;
        // The backend records straight into the native command list, past CommandContext's batched barriers.
        // None may be waiting (asserted): the caller flushes them first.
        ENGINE_ASSERT(!context.HasPendingBarriers());
        // Also creates or updates the font texture first if ImGui asks for it (startup, new glyphs)
        ImGui_ImplDX12_RenderDrawData(drawData, context.Native());
    }

    void UiRenderer::AllocateDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu,
                                        D3D12_GPU_DESCRIPTOR_HANDLE *gpu) {
        auto *self = static_cast<UiRenderer *>(info->UserData);
        // The backend writes the texture's SRV through the CPU handle and binds the GPU handle as a descriptor
        // table, so it gets handles, not the index
        const uint32_t index = self->m_heap.Allocate();
        *cpu = self->m_heap.CpuHandle(index);
        *gpu = self->m_heap.GpuHandle(index);
    }

    void UiRenderer::FreeDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                    D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/) {
        auto *self = static_cast<UiRenderer *>(info->UserData);
        // Textures are created only inside Render, so a frame is known by the time one is freed
        ENGINE_ASSERT_MSG(self->m_frame != nullptr, "ImGui freed a descriptor before the first UiRenderer::Render");
        // The backend frees a texture only after it went unused for NumFramesInFlight frames, but the slot still
        // waits for the fence like every other bindless index (rhi rule: never Free() directly)
        self->m_frame->DeferFree(self->m_heap, self->m_heap.IndexOf(cpu));
    }
} // namespace engine::render
