#ifndef D3D12_ENGINE_RENDERER_H
#define D3D12_ENGINE_RENDERER_H

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/BindlessHeap.h"
#include "rhi/CommandContext.h"
#include "rhi/CommandQueue.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/Device.h"
#include "rhi/FrameContext.h"
#include "rhi/GpuProfiler.h"
#include "rhi/PipelineCache.h"
#include "rhi/SwapChain.h"
#include "rhi/UploadContext.h"
#include "rhi/ValidationStats.h"

#include "render/DynamicInstances.h"
#include "render/FrameGraph.h"
#include "render/GpuResources.h"
#include "render/TransientTexturePool.h"
#include "render/passes/OpaquePass.h"
#include "render/passes/TransparentPass.h"
#include "renderdata/FrameView.h"
#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
#include "renderdata/RenderList.h"
#include "renderdata/RendererSettings.h"
#include "renderdata/TransparentSort.h"

// ImGui's per-frame draw data (ui::DebugUI::Render); opaque here
struct ImDrawData;

namespace engine::render {
    class UiRenderer;

    // Owns the GPU objects of the window's frame loop and records one frame at a time.
    //
    // Per frame: BeginFrame(width, height, settings) -> Render(view, lights, list, staticLists, ui) -> EndFrame(),
    // each once and in this order (asserted). Any of them can throw core::HResultError. If the GPU was lost, the
    // destructor logs why (and the DRED info in Debug builds).
    //
    // The frame is recorded by a frame graph (FrameGraph), built on the first frame and again when the back
    // buffer's size changes or the UI is enabled. The scene depth is one of the graph's textures, the size of the
    // back buffer.
    //
    // Not copyable or movable (the swap chain and descriptors keep references to members).
    // Asset loading creates meshes, textures and materials through Resources().
    class Renderer final {
    public:
        // `validation` collects debug-layer errors and leaks (Debug builds) and must outlive the renderer:
        // the device writes the live-object count when it is destroyed, after everything else
        Renderer(HWND hwnd, uint32_t width, uint32_t height, rhi::ValidationStats &validation);
        // Waits for the GPU before any member is released
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        // Waits until this frame's slot is free, matches the swap chain to the window's client size
        // (no-op when unchanged), takes this frame's settings and starts recording
        void BeginFrame(uint32_t width, uint32_t height, const RendererSettings &settings);

        // Width / height of the back buffers, for the camera's projection. Valid after BeginFrame; never
        // 0 or NaN, because the swap chain keeps its last size while the window is minimized.
        [[nodiscard]] float AspectRatio() const noexcept {
            return static_cast<float>(m_swapChain.Width()) / static_cast<float>(m_swapChain.Height());
        }

        // Records the frame: draws the static lists, then `list` (uploaded again every frame), seen from `view`
        // and lit by `lights`, transparent objects last, then the UI's draw data `ui` (needs EnableUi; null: no UI)
        void Render(const FrameView &view, const LightSet &lights, const RenderList &list,
                    std::span<const StaticListHandle> staticLists = {}, ImDrawData *ui = nullptr);

        // The meshes, textures, materials and static lists the renderer draws. Pass it to AssetCache as the
        // GpuUploader. Everything created through it lives as long as the renderer; new resources are uploaded
        // before the next frame is submitted (EndFrame), so they can be drawn in the frame that created them.
        [[nodiscard]] GpuResources &Resources() noexcept { return m_resources; }

        // Starts the debug UI's renderer (ImGui's DirectX 12 backend). Call once, after the ImGui context exists
        // (ui::DebugUI); the context must outlive the renderer. Apps without UI (the bench) never call it.
        void EnableUi();

        // Submits the frame, presents it (with this frame's RendererSettings::vsync) and remembers the fence value
        // that marks its end
        void EndFrame();

        [[nodiscard]] const std::optional<rhi::GpuFrameTimes> &GpuTimes() const noexcept { return m_profiler.Latest(); }

        [[nodiscard]] std::vector<rhi::GpuFrameTimes> FinishGpuTimes();

        [[nodiscard]] bool TearingSupported() const noexcept { return m_device.TearingSupported(); }
        // For benchmark results
        [[nodiscard]] const std::string &AdapterName() const noexcept { return m_device.AdapterName(); }
        [[nodiscard]] const std::string &DriverVersion() const noexcept { return m_device.DriverVersion(); }

    private:
        // The last frame call made, for the asserts on their order
        enum class FrameStage : uint8_t {
            Idle, // before BeginFrame, or after EndFrame
            Begun,
            Rendered,
        };

        // What the frame graph's passes depend on. When it changes, the graph is built again.
        struct GraphKey {
            uint32_t width = 0;
            uint32_t height = 0;
            bool ui = false;

            friend bool operator==(const GraphKey &, const GraphKey &) = default;
        };

        [[nodiscard]] rhi::FrameContext &CurrentFrame() { return m_frames[m_frameNumber % rhi::kFramesInFlight]; }

        // Adds the passes to m_graph and compiles it
        void BuildGraph();

        // Shutdown only: logs why the device was removed (if it was), waits for the GPU, then destroys every
        // texture (the frame graph's too), mesh buffer and static list
        void DestroyResources() noexcept;

        // Members are destroyed in reverse order. The device is first, so it goes last and its
        // live-object report sees everything else released. The resource allocator is next: every
        // buffer, texture and deferred release (in the frames) must be gone before it. The frames, the
        // upload context and the command list come before the queue: the queue's destructor waits for
        // the GPU before they are released, even when the constructor throws. Members that keep references
        // to others (m_resources, the swap chain) come after them.
        rhi::Device m_device;
        rhi::Allocator m_allocator;
        // Before the frames: their deferred frees return indices to it, also when they are destroyed
        rhi::BindlessHeap m_bindlessHeap;
        std::vector<rhi::FrameContext> m_frames;
        // Uploads of meshes and textures
        rhi::UploadContext m_upload;
        rhi::CommandContext m_context;
        rhi::CommandQueue m_queue;
        rhi::GpuProfiler m_profiler;
        // Back buffers and other render targets; 64 slots cost a few KB of CPU memory
        rhi::DescriptorAllocator m_rtvAllocator;
        // Scene depth and shadow maps
        rhi::DescriptorAllocator m_dsvAllocator;
        // After the queue and the allocator: its destructor flushes the queue and returns its RTVs
        rhi::SwapChain m_swapChain;
        // The frame graph's textures (scene depth). After the allocators and the bindless heap, whose slots its
        // textures hold. DestroyResources hands its textures to a frame, which releases them before the allocator
        // goes.
        TransientTexturePool m_transients;

        // The global root signature and every PSO; the passes keep handles into it
        rhi::PipelineCache m_pipelines;

        // After the allocator, the upload context, the queue and the bindless heap, which it uses. Its buffers and
        // textures are handed to a frame by DestroyResources (after the destructor's flush), so the frames
        // release them, before the allocator goes.
        GpuResources m_resources;
        // After the pipeline cache, which they keep a reference to
        OpaquePass m_opaque;
        TransparentPass m_transparent;
        // This frame's moving objects; CPU memory only (the instance data is in the frames' upload rings)
        DynamicInstances m_dynamicInstances;

        // The UI's renderer; null until EnableUi. Released after the destructor's flush, before the frames and the
        // bindless heap: its shutdown hands the font texture's descriptor to a frame's deferred frees.
        std::unique_ptr<UiRenderer> m_ui;

        // After everything its passes keep references to (the opaque pass, the resources, the UI's renderer), so it
        // is destroyed first
        FrameGraph m_graph;
        std::optional<GraphKey> m_graphKey; // what m_graph was built for; empty before the first frame
        FgTexture m_backBuffer;
        // This frame's opaque draws: the static lists' and the moving objects'. Kept so its memory is reused.
        std::vector<std::span<const InstanceDraw>> m_drawSets;
        // This frame's, sorted back to front; kept so its memory is reused
        std::vector<TransparentObject> m_transparentObjects;

        RendererSettings m_settings; // this frame's copy, taken in BeginFrame
        uint64_t m_frameNumber = 0;
        FrameStage m_frameStage = FrameStage::Idle;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERER_H
