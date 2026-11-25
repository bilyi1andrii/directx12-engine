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

namespace render {
    using core::ThrowIfFailed;

    namespace {
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

        constexpr uint32_t kBackBufferSrgbView = 0;
        constexpr uint32_t kBackBufferUnormView = 1;
    } // namespace

    Renderer::Renderer(HWND hwnd, uint32_t width, uint32_t height, rhi::ValidationStats &validation)
        : m_device(validation), m_allocator(m_device),
          m_bindlessHeap(m_device.Native(), rhi::BindlessHeap::kDefaultCapacity, L"Bindless heap"),
          m_upload(m_device.Native(), m_allocator, L"Upload"),
          m_context(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list"),
          m_queue(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue"),
          m_profiler(m_device.Native(), m_allocator, m_queue.TimestampFrequency()),
          m_rtvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap"),
          m_dsvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, L"DSV heap"),
          m_swapChain(m_device, m_queue, m_rtvAllocator, hwnd, width, height),
          m_transients(m_device.Native(), m_allocator, m_rtvAllocator, m_dsvAllocator, m_bindlessHeap),
          m_pipelines(m_device.Native(), core::ExecutableDirectory() / L"shaders"),
          m_resources(m_allocator, m_upload, m_queue, m_bindlessHeap), m_shadow(m_pipelines), m_opaque(m_pipelines),
          m_transparent(m_pipelines), m_graph(m_pipelines.RootSignature(), m_profiler) {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), m_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  std::format(L"Frame {}", i));
        }

        m_upload.Begin(m_queue);
        try {
            m_resources.CreateDefaults();
            m_upload.Submit(m_queue);
        } catch (...) {
            DestroyResources();
            throw;
        }
    }

    Renderer::~Renderer() {
        DestroyResources();
    }

    void Renderer::DestroyResources() noexcept {
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
        m_resources.DestroyAll(CurrentFrame());
        m_transients.DestroyAll(CurrentFrame());
    }

    void Renderer::BeginFrame(uint32_t width, uint32_t height, const RendererSettings &settings) {
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Idle, "BeginFrame before EndFrame of frame {}", m_frameNumber);
        m_frameStage = FrameStage::Begun;
        m_settings = settings;

        rhi::FrameContext &frame = CurrentFrame();
        frame.WaitAndReset(m_queue);
        m_upload.ReleaseCompleted(m_queue);

        m_swapChain.Resize(width, height);

        m_context.Begin(frame.CommandAllocator());
        m_context.SetDescriptorHeap(m_bindlessHeap.Native());
        m_profiler.BeginFrame(m_context, m_frameNumber);
    }

    void Renderer::Render(const FrameView &view, const LightSet &lights, const RenderList &list,
                          std::span<const StaticListHandle> staticLists, ImDrawData *ui) {
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Begun, "Render must run once, after BeginFrame, in frame {}",
                          m_frameNumber);
        m_frameStage = FrameStage::Rendered;

        const gpu::FrameConstants frameConstants = MakeFrameConstants(view, lights);
        const rhi::UploadAllocation frameCb =
            CurrentFrame().Ring().Write(frameConstants, rhi::kConstantBufferAlignment);

        m_dynamicInstances.Upload(list, CurrentFrame(), m_bindlessHeap);
        m_drawSets.clear();
        for (const StaticListHandle handle : staticLists) {
            m_drawSets.emplace_back(m_resources.StaticList(handle).draws);
        }
        m_drawSets.emplace_back(m_dynamicInstances.Draws());

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
                                 .shadow =
                                     MakeShadowView(lights.environment.directional.direction, lights.shadowBounds),
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

        const FgTexture shadowMap = m_graph.CreateTexture("ShadowMap", {.width = kShadowMapSize,
                                                                        .height = kShadowMapSize,
                                                                        .format = rhi::kDepthFormat,
                                                                        .target = TransientTextureDesc::Target::Depth,
                                                                        .shaderRead = true});

        m_shadow.AddTo(m_graph, shadowMap, m_resources);
        m_opaque.AddTo(m_graph, m_backBuffer, kBackBufferSrgbView, sceneDepth, shadowMap, m_resources);
        m_transparent.AddTo(m_graph, m_backBuffer, kBackBufferSrgbView, sceneDepth, shadowMap, m_resources);
        if (m_ui) {
            AddUiPass(m_graph, m_backBuffer, kBackBufferUnormView, *m_ui);
        }
        m_graph.Compile(m_transients, CurrentFrame());
    }

    void Renderer::EnableUi() {
        m_ui = std::make_unique<UiRenderer>(m_device.Native(), m_queue.Native(), m_bindlessHeap);
    }

    std::vector<rhi::GpuFrameTimes> Renderer::FinishGpuTimes() {
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Idle, "FinishGpuTimes inside frame {}", m_frameNumber);
        m_queue.Flush();
        return m_profiler.ReadRemaining();
    }

    void Renderer::EndFrame() {
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Rendered, "EndFrame without Render in frame {}", m_frameNumber);
        m_frameStage = FrameStage::Idle;

        m_profiler.EndFrame(m_context);
        m_context.Close();

        if (m_upload.IsRecording()) {
            m_upload.Submit(m_queue);
        }

        m_queue.ExecuteCommandList(m_context.Native());
        m_swapChain.Present(m_settings.vsync);
        CurrentFrame().SetFenceValue(m_queue.Signal());
        m_device.DrainDxgiMessages();
        ++m_frameNumber;
    }
} // namespace render
