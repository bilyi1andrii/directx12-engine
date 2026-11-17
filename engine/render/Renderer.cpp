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
#include "render/UiRenderer.h"
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
          m_depth(m_device.Native(), m_allocator, m_dsvAllocator, width, height, L"Depth buffer"),
          m_pipelines(m_device.Native(), core::ExecutableDirectory() / L"shaders"),
          m_resources(m_allocator, m_upload, m_queue, m_bindlessHeap), m_opaque(m_pipelines) {
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
        // swap chain (depth buffer, PSOs, GPU resources) are released before its flush, so wait
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

        // After WaitAndReset: objects a resize defers into this slot must survive until this
        // frame's fence, not be dropped by the reset
        if (m_swapChain.Resize(width, height)) {
            m_depth.Resize(m_swapChain.Width(), m_swapChain.Height(), frame);
        }

        m_context.Begin(frame.CommandAllocator());
        // Before any root signature is set: shaders index this heap (bindless), and only one can be bound
        m_context.SetDescriptorHeap(m_bindlessHeap.Native());
    }

    void Renderer::Render(const FrameView &view, const LightSet &lights, const RenderList &list,
                          std::span<const StaticListHandle> staticLists) {
        // Render makes the back buffer writable, and that transition must happen once per frame
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Begun, "Render must run once, after BeginFrame, in frame {}",
                          m_frameNumber);
        m_frameStage = FrameStage::Rendered;

        ID3D12Resource *backBuffer = m_swapChain.CurrentBackBuffer();
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.CurrentRtv();

        // The sky: linear values, so through the sRGB view the screen shows about (56, 69, 108) of 255
        constexpr std::array clearColor{0.04f, 0.06f, 0.15f, 1.0f};
        m_context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_context.ClearRenderTarget(rtv, clearColor);
        // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
        m_context.ClearDepth(m_depth.Dsv(), rhi::DepthBuffer::kClearDepth);

        m_context.SetRenderTarget(rtv, m_depth.Dsv());
        m_context.SetViewportAndScissor(m_swapChain.Width(), m_swapChain.Height());
        m_context.SetGraphicsRootSignature(m_pipelines.RootSignature());

        // Per-frame constants go into this frame slot's upload ring: the GPU reads them while the CPU
        // already records the next frame into the other slot's ring
        const gpu::FrameConstants frameConstants = MakeFrameConstants(view, lights);
        const rhi::UploadAllocation frameCb =
            CurrentFrame().Ring().Write(frameConstants, rhi::kConstantBufferAlignment);
        m_context.SetGraphicsRootConstantBufferView(gpu::kRootFrameConstants, frameCb.gpu);

        m_dynamicInstances.Upload(list, CurrentFrame(), m_bindlessHeap);
        // Static lists first, then the dynamic objects
        for (const StaticListHandle handle : staticLists) {
            m_opaque.Record(m_context, m_resources, m_settings.drawPath, m_resources.StaticList(handle).draws);
        }
        m_opaque.Record(m_context, m_resources, m_settings.drawPath, m_dynamicInstances.Draws());
    }

    void Renderer::EnableUi() {
        ENGINE_ASSERT_MSG(m_ui == nullptr, "EnableUi called twice");
        m_ui = std::make_unique<UiRenderer>(m_device.Native(), m_queue.Native(), m_bindlessHeap);
    }

    void Renderer::RenderUi(ImDrawData *drawData) {
        ENGINE_ASSERT_MSG(m_ui != nullptr, "RenderUi without EnableUi");
        // A second call would make the backend reuse a vertex buffer the GPU may still be reading
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Rendered, "RenderUi must run once, after Render, in frame {}",
                          m_frameNumber);
        m_frameStage = FrameStage::UiRendered;
        // Last in the frame: ImGui sets its own root signature and PSO, and nothing after it needs ours.
        // The UNORM view, because ImGui's colours are already sRGB.
        m_ui->Render(drawData, m_context, CurrentFrame(), m_swapChain.CurrentUiRtv());
    }

    void Renderer::EndFrame() {
        // Render must have made the back buffer writable: EndFrame turns it back
        ENGINE_ASSERT_MSG(m_frameStage == FrameStage::Rendered || m_frameStage == FrameStage::UiRendered,
                          "EndFrame without Render in frame {}", m_frameNumber);
        m_frameStage = FrameStage::Idle;

        m_context.Transition(m_swapChain.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_PRESENT);
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
