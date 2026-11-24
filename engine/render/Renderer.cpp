#include "render/Renderer.h"

#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <span>

#include <DirectXMath.h>
#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "render/FrameInputs.h"
#include "rhi/DepthFormat.h"
#include "render/UiRenderer.h"
#include "render/passes/UiPass.h"
#include "renderdata/LightPacking.h"
#include "rhi/UploadRing.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace engine::render {
    using core::ThrowIfFailed;

    namespace {
        // The camera and the lights in the layout the shaders read. Matrices are transposed: DirectXMath uses row
        // vectors, and HLSL reads matrices column-major, so shaders compute mul(v, M).
        gpu::FrameConstants MakeFrameConstants(const FrameView &view, const LightSet &lights) {
            using namespace DirectX;

            const XMMATRIX viewMatrix = XMLoadFloat4x4(&view.view);
            const XMMATRIX projection = XMLoadFloat4x4(&view.projection);

            gpu::FrameConstants constants{};
            XMStoreFloat4x4(&constants.view, XMMatrixTranspose(viewMatrix));
            XMStoreFloat4x4(&constants.projection, XMMatrixTranspose(projection));
            XMStoreFloat4x4(&constants.viewProjection, XMMatrixTranspose(viewMatrix * projection));
            constants.cameraPosition = view.cameraPosition;
            PackLights(lights, constants);
            return constants;
        }

        // The back buffer's two render-target views, as numbered in its TextureBinding. The scene draws through
        // the sRGB view; the UI through the UNORM one, because ImGui's colours are already sRGB.
        constexpr uint32_t kBackBufferSrgbView = 0;
        constexpr uint32_t kBackBufferUnormView = 1;
    } // namespace

    Renderer::Renderer(HWND hwnd, uint32_t width, uint32_t height, rhi::ValidationStats &validation)
        : m_device(validation), m_allocator(m_device),
          m_bindlessHeap(m_device.Native(), rhi::BindlessHeap::kDefaultCapacity, L"Bindless heap"),
          m_upload(m_device.Native(), m_allocator, L"Upload"),
          m_context(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list"),
          m_queue(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue"),
          m_rtvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap"),
          m_dsvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, L"DSV heap"),
          m_swapChain(m_device, m_queue, m_rtvAllocator, hwnd, width, height),
          m_transients(m_device.Native(), m_allocator, m_rtvAllocator, m_dsvAllocator, m_bindlessHeap),
          m_pipelines(m_device.Native(), core::ExecutableDirectory() / L"shaders"),
          m_resources(m_allocator, m_upload, m_queue, m_bindlessHeap), m_opaque(m_pipelines),
          m_transparent(m_pipelines), m_graph(m_pipelines.RootSignature()) {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), m_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  std::format(L"Frame {}", i));
        }

        // All startup uploads in one batch, submitted before the first frame
        m_upload.Begin(m_queue);
        try {
            m_resources.CreateDefaults();
            m_upload.Submit(m_queue);
        } catch (...) {
            // ~Renderer does not run when the constructor throws, so clean up the textures and buffers here
            DestroyResources();
            throw;
        }
    }

    Renderer::~Renderer() {
        // The destructor body runs before any member is destroyed. Members declared after the
        // swap chain (the frame graph's textures, PSOs, GPU resources) are released before its flush, so wait
        // here (DestroyResources flushes).
        DestroyResources();
    }

    void Renderer::DestroyResources() noexcept {
        // An exception that ends the app destroys the renderer on its way out, so a lost GPU is logged here.
        // Before the flush, which fails once the device is removed.
        try {
            m_device.LogDeviceRemovedInfo();
        } catch (const std::exception &e) {
            core::LogError("Renderer: logging why the device was removed failed: {}", e.what());
        }
        try {
            m_queue.Flush();
        } catch (const std::exception &e) {
            core::LogError("Renderer: flush at destruction failed: {}", e.what());
        }
        // Hands every texture, mesh buffer and static list to a frame slot; the slots free them when the renderer
        // is destroyed
        m_resources.DestroyAll(CurrentFrame());
        m_transients.DestroyAll(CurrentFrame());
    }

    void Renderer::BeginFrame(uint32_t width, uint32_t height, const RendererSettings &settings) {
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Idle, "BeginFrame before EndFrame of frame {}", m_frameNumber);
        m_frameStage = FrameStage::Begun;
        m_settings = settings;

        // This slot was last used kFramesInFlight frames ago; wait until the GPU has finished that
        // frame before reusing its allocator
        rhi::FrameContext &frame = CurrentFrame();
        frame.WaitAndReset(m_queue);
        // Frees the staging memory of a finished upload batch
        m_upload.ReleaseCompleted(m_queue);

        // Targets of the back buffer's size follow in Render: a new size rebuilds the frame graph, which acquires new
        // textures from the pool and hands the old ones to this frame slot
        m_swapChain.Resize(width, height);

        m_context.Begin(frame.CommandAllocator());
        // Before any root signature is set: shaders index this heap (bindless), and only one can be bound
        m_context.SetDescriptorHeap(m_bindlessHeap.Native());
    }

    void Renderer::Render(const FrameView &view, const LightSet &lights, const RenderList &list,
                          std::span<const StaticListHandle> staticLists, ImDrawData *ui) {
        // Once per frame: a second call would write the dynamic instances and the UI's vertex buffer again, and the
        // UI backend would reuse a buffer the GPU may still be reading
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Begun, "Render must run once, after BeginFrame, in frame {}",
                          m_frameNumber);
        ENGINE_ASSERT_MSG(ui == nullptr || m_ui != nullptr, "UI draw data without EnableUi");
        m_frameStage = FrameStage::Rendered;

        // Per-frame constants go into this frame slot's upload ring: the GPU reads them while the CPU
        // already records the next frame into the other slot's ring
        const gpu::FrameConstants frameConstants = MakeFrameConstants(view, lights);
        const rhi::UploadAllocation frameCb =
            CurrentFrame().Ring().Write(frameConstants, rhi::kConstantBufferAlignment);

        m_dynamicInstances.Upload(list, CurrentFrame(), m_bindlessHeap);
        // Static lists first, then the dynamic objects
        m_drawSets.clear();
        for (const StaticListHandle handle : staticLists) {
            m_drawSets.emplace_back(m_resources.StaticList(handle).draws);
        }
        m_drawSets.emplace_back(m_dynamicInstances.Draws());

        // Transparent objects of both kinds in one order. Each one's depth is computed once, here.
        m_transparentObjects.clear();
        for (const StaticListHandle handle : staticLists) {
            const GpuStaticList &staticList = m_resources.StaticList(handle);
            for (const StaticTransparentObject &object : staticList.transparent) {
                m_transparentObjects.push_back({.key = object.key,
                                                .instanceBufferIndex = staticList.instanceBufferIndex,
                                                .element = object.element,
                                                .viewDepth = ViewDepth(view.view, object.position)});
            }
        }
        m_dynamicInstances.AppendTransparent(list, m_resources, view.view, m_transparentObjects);
        SortBackToFront(m_transparentObjects);

        const GraphKey key{.width = m_swapChain.Width(), .height = m_swapChain.Height(), .ui = m_ui != nullptr};
        if (m_graphKey != key) {
            BuildGraph();
            m_graphKey = key;
        }

        const std::array imports{
            TextureBinding{.texture = m_backBuffer,
                           .resource = m_swapChain.CurrentBackBuffer(),
                           .views = {m_swapChain.CurrentRtv(), m_swapChain.CurrentUiRtv()},
                           .width = m_swapChain.Width(),
                           .height = m_swapChain.Height()},
        };
        const FrameInputs inputs{.frameConstants = frameCb.gpu,
                                 .drawPath = m_settings.drawPath,
                                 .drawSets = m_drawSets,
                                 .transparentObjects = m_transparentObjects,
                                 .ui = ui};
        m_graph.Execute(m_context, CurrentFrame(), imports, inputs);
    }

    void Renderer::BuildGraph() {
        m_graph.Reset();
        m_backBuffer = m_graph.Import("BackBuffer", fg::Access::Present, fg::Access::Present);
        const FgTexture sceneDepth = m_graph.CreateTexture("SceneDepth", {.width = m_swapChain.Width(),
                                                                          .height = m_swapChain.Height(),
                                                                          .format = rhi::kDepthFormat,
                                                                          .target = TransientTextureDesc::Target::Depth,
                                                                          .shaderRead = false});

        m_opaque.AddTo(m_graph, m_backBuffer, kBackBufferSrgbView, sceneDepth, m_resources);
        // After the solid surfaces, which it blends over and tests depth against
        m_transparent.AddTo(m_graph, m_backBuffer, kBackBufferSrgbView, sceneDepth, m_resources);
        if (m_ui) {
            // Last: on top of the scene
            AddUiPass(m_graph, m_backBuffer, kBackBufferUnormView, *m_ui);
        }
        m_graph.Compile(m_transients, CurrentFrame());
    }

    void Renderer::EnableUi() {
        ENGINE_ASSERT_MSG(m_ui == nullptr, "EnableUi called twice");
        m_ui = std::make_unique<UiRenderer>(m_device.Native(), m_queue.Native(), m_bindlessHeap);
    }

    void Renderer::EndFrame() {
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Rendered, "EndFrame without Render in frame {}", m_frameNumber);
        m_frameStage = FrameStage::Idle;

        // The frame graph has left the back buffer in PRESENT
        m_context.Close();

        // Uploads first: this frame may already use them, and the GPU runs lists in submission order
        if (m_upload.IsRecording()) {
            m_upload.Submit(m_queue);
        }

        // Submit, present, and remember which fence value marks the end of this frame's GPU work
        m_queue.ExecuteCommandList(m_context.Native());
        m_swapChain.Present(m_settings.vsync);
        CurrentFrame().SetFenceValue(m_queue.Signal());
        // Logs this frame's DXGI messages (Present, ResizeBuffers) next to the frame that caused them
        m_device.DrainDxgiMessages();
        ++m_frameNumber;
    }
} // namespace engine::render
