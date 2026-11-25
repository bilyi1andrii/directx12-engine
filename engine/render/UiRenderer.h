#ifndef D3D12_ENGINE_UIRENDERER_H
#define D3D12_ENGINE_UIRENDERER_H

#include "rhi/BindlessHeap.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/RhiCommon.h"

// ImGui types, defined in imgui.h / imgui_impl_dx12.h (included only by UiRenderer.cpp)
struct ImDrawData;
struct ImGui_ImplDX12_InitInfo;

namespace render {
    // Draws Dear ImGui's draw data with ImGui's official DirectX 12 backend (imgui_impl_dx12, used unmodified).
    // The backend brings its own root signature and PSO (shaders compiled at runtime with FXC), per-frame
    // vertex/index buffers and font texture (committed resources, outside rhi::Allocator). Its descriptors come
    // from BindlessHeap, the one shader-visible heap.
    //
    // Needs the ImGui context (ui::DebugUI) for its whole life. Not copyable or movable.
    class UiRenderer {
    public:
        // Initializes the backend and creates its root signature and PSO. The font texture is created and
        // uploaded by the first Render. Throws std::runtime_error on failure.
        UiRenderer(ID3D12Device *device, ID3D12CommandQueue *queue, rhi::BindlessHeap &heap);
        // Shuts the backend down and releases its resources at once: the GPU must be idle (Renderer flushes
        // first). The descriptors go to the last rendered frame's deferred frees, which must still exist.
        ~UiRenderer();

        UiRenderer(const UiRenderer &) = delete;
        UiRenderer &operator=(const UiRenderer &) = delete;
        UiRenderer(UiRenderer &&) = delete;
        UiRenderer &operator=(UiRenderer &&) = delete;

        // Records `drawData` into `context`, on top of the bound render target: a back buffer through its kFormat
        // (UNORM) view, in RENDER_TARGET state, with no barriers still queued. In the renderer, the frame graph's UI
        // pass binds it and flushes. Leaves the backend's root signature and PSO set.
        // At most once per frame: the backend switches to its next vertex/index buffer on every call, and with a
        // second call it would overwrite one the GPU may still read. Descriptors the backend frees meanwhile
        // (atlas textures it replaces) go to `frame`'s deferred frees.
        void Render(ImDrawData *drawData, rhi::CommandContext &context, rhi::FrameContext &frame);

    private:
        // The backend's descriptor callbacks; InitInfo::UserData is the UiRenderer
        static void AllocateDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu,
                                       D3D12_GPU_DESCRIPTOR_HANDLE *gpu);
        static void FreeDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                   D3D12_GPU_DESCRIPTOR_HANDLE gpu);

        rhi::BindlessHeap &m_heap;
        // The frame Render last recorded into; receives the descriptors the backend frees
        rhi::FrameContext *m_frame = nullptr;
    };
} // namespace render

#endif // D3D12_ENGINE_UIRENDERER_H
