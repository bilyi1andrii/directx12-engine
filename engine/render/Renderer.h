#ifndef D3D12_ENGINE_RENDERER_H
#define D3D12_ENGINE_RENDERER_H

// Draws the window's frames: it owns the device, the swap chain and every GPU object, and records one frame at a
// time through the frame graph. Apps create meshes, textures and materials through its resources.

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
#include "render/passes/ShadowPass.h"
#include "render/passes/TransparentPass.h"
#include "renderdata/FrameView.h"
#include "renderdata/Handles.h"
#include "renderdata/Lights.h"
#include "renderdata/RenderList.h"
#include "renderdata/RendererSettings.h"
#include "renderdata/TransparentSort.h"

struct ImDrawData;

namespace render {
    class UiRenderer;

    class Renderer final {
    public:
        Renderer(HWND hwnd, uint32_t width, uint32_t height, rhi::ValidationStats &validation);
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        void BeginFrame(uint32_t width, uint32_t height, const RendererSettings &settings);

        [[nodiscard]] float AspectRatio() const noexcept {
            return static_cast<float>(m_swapChain.Width()) / static_cast<float>(m_swapChain.Height());
        }

        void Render(const FrameView &view, const LightSet &lights, const RenderList &list,
                    std::span<const StaticListHandle> staticLists = {}, ImDrawData *ui = nullptr);

        [[nodiscard]] GpuResources &Resources() noexcept { return m_resources; }

        void EnableUi();

        void EndFrame();

        [[nodiscard]] const std::optional<rhi::GpuFrameTimes> &GpuTimes() const noexcept { return m_profiler.Latest(); }

        // flushes queue, returns frames still in flight
        [[nodiscard]] std::vector<rhi::GpuFrameTimes> FinishGpuTimes();

        [[nodiscard]] bool TearingSupported() const noexcept { return m_device.TearingSupported(); }
        [[nodiscard]] const std::string &AdapterName() const noexcept { return m_device.AdapterName(); }
        [[nodiscard]] const std::string &DriverVersion() const noexcept { return m_device.DriverVersion(); }

    private:
        // enforces BeginFrame, Render, EndFrame order
        enum class FrameStage : uint8_t {
            Idle,
            Begun,
            Rendered,
        };

        // graph rebuilt when this changes
        struct GraphKey {
            uint32_t width = 0;
            uint32_t height = 0;
            bool ui = false;

            friend bool operator==(const GraphKey &, const GraphKey &) = default;
        };

        // same slot as two frames ago
        [[nodiscard]] rhi::FrameContext &CurrentFrame() { return m_frames[m_frameNumber % rhi::kFramesInFlight]; }

        void BuildGraph();

        void DestroyResources() noexcept;

        // destroyed bottom to top, so each member outlives what uses it
        rhi::Device m_device;             // last to go, its leak report sees everything released
        rhi::Allocator m_allocator;       // outlives every buffer, texture and deferred release
        rhi::BindlessHeap m_bindlessHeap; // outlives frames, their deferred frees return slots here
        std::vector<rhi::FrameContext> m_frames;
        rhi::UploadContext m_upload;
        rhi::CommandContext m_context;
        rhi::CommandQueue m_queue; // goes before frames, upload and list, its destructor waits for GPU
        rhi::GpuProfiler m_profiler;
        rhi::DescriptorAllocator m_rtvAllocator;
        rhi::DescriptorAllocator m_dsvAllocator;
        rhi::SwapChain m_swapChain;        // uses queue and RTV allocator
        TransientTexturePool m_transients; // its textures use allocators and heap
        rhi::PipelineCache m_pipelines;
        GpuResources m_resources; // uses everything above
        ShadowPass m_shadow;      // passes refer to pipeline cache
        OpaquePass m_opaque;
        TransparentPass m_transparent;
        DynamicInstances m_dynamicInstances;
        std::unique_ptr<UiRenderer> m_ui;   // destroyed before frames and heap, its shutdown frees a slot through them
        FrameGraph m_graph;                 // first to go, its passes refer to members above
        std::optional<GraphKey> m_graphKey; // empty until first build
        FgTexture m_backBuffer;
        std::vector<std::span<const InstanceDraw>> m_drawSets; // static lists then moving objects
        std::vector<TransparentObject> m_transparentObjects;

        RendererSettings m_settings;
        uint64_t m_frameNumber = 0;
        FrameStage m_frameStage = FrameStage::Idle;
    };
} // namespace render

#endif // D3D12_ENGINE_RENDERER_H
