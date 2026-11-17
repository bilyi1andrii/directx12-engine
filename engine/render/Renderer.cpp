#include "render/Renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "render/InstanceUpload.h"
#include "render/UiRenderer.h"
#include "renderdata/LightPacking.h"
#include "renderdata/Vertex.h"
#include "rhi/UploadRing.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace engine::render {
    using core::ThrowIfFailed;

    namespace {
        static_assert(sizeof(gpu::DrawConstants) <= gpu::kDrawConstantCount * sizeof(uint32_t),
                      "DrawConstants must fit in the root constants of b0 (common/root_signature.hlsli)");
        // Position of DrawConstants::instanceOffset in b0, in 32-bit values
        constexpr uint32_t kInstanceOffsetConstant = offsetof(gpu::DrawConstants, instanceOffset) / sizeof(uint32_t);

        // How the GPU reads one Vertex (renderdata/Vertex.h): must match VSInput in vs_mesh.hlsl
        constexpr std::array kVertexLayout{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, position),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TEXCOORD",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, uv),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "NORMAL",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, normal),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TANGENT",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, tangent),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
        };

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
          m_resources(m_allocator, m_upload, m_queue, m_bindlessHeap) {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), m_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  std::format(L"Frame {}", i));
        }
        CreatePipelines();

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

        UploadDynamicInstances(list);
        m_context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // Static lists first, then the dynamic objects
        for (const StaticListHandle handle : staticLists) {
            RecordDraws(m_resources.StaticList(handle).draws);
        }
        RecordDraws(m_dynamicDraws);
    }

    void Renderer::UploadDynamicInstances(const RenderList &list) {
        rhi::FrameContext &frame = CurrentFrame();
        const auto beginChunk = [&](uint32_t elementCount) {
            // Aligned to the element size, so the view can start at a whole element
            InstanceChunk chunk{.memory =
                                    frame.Ring().Allocate(uint64_t{elementCount} * kInstanceStride, kInstanceStride)};
            // A fresh bindless slot every frame: the previous frame may still read the old one. It is freed after
            // this frame's fence.
            chunk.instanceBufferIndex = m_bindlessHeap.Allocate();
            frame.DeferFree(m_bindlessHeap, chunk.instanceBufferIndex);
            m_bindlessHeap.CreateStructuredBufferSrv(chunk.instanceBufferIndex, chunk.memory.resource,
                                                     chunk.memory.offset / kInstanceStride, elementCount,
                                                     kInstanceStride);
            return chunk;
        };
        // A shader view (SRV) covers one buffer, and every ring page is its own buffer: chunks of at most a page
        m_dynamicDraws.clear();
        WriteInstances(list, static_cast<uint32_t>(frame.Ring().PageSize() / kInstanceStride), beginChunk,
                       m_instanceRanges, m_dynamicDraws);
    }

    void Renderer::RecordDraws(std::span<const InstanceDraw> draws) {
        // Root constants (b0) are copied into the command list, so every draw keeps its own values
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        for (const InstanceDraw &draw : draws) {
            const GpuMesh &mesh = m_resources.Mesh(draw.key.mesh);
            const GpuGeometry &geometry = m_resources.Geometry(mesh.geometry);
            const MaterialInfo &material = m_resources.Material(draw.key.material);
            // Switch PSOs only when needed: it costs CPU and GPU time. The PSO depends on the geometry's vertex
            // format and on the material, so it is checked for every draw.
            const rhi::PipelineHandle pipeline = PipelineFor(geometry.vertexFormat, material);
            if (pipeline != currentPipeline) {
                m_context.SetPipelineState(m_pipelines.Native(pipeline));
                currentPipeline = pipeline;
            }
            // The meshes of one model share their buffers, and a bucket split between two chunks gives two draws
            // of the same mesh: bind the buffers only when the geometry changes
            if (mesh.geometry != currentGeometry) {
                m_context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                m_context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            // Every draw reads indexCount indices from firstIndex on, and the GPU adds baseVertex to each
            gpu::DrawConstants constants{.instanceOffset = draw.firstElement,
                                         .materialIndex = material.albedo.index,
                                         .instanceBufferIndex = draw.instanceBufferIndex};
            if (m_settings.drawPath == DrawPath::Instanced) {
                // One draw for all `count` objects: object i reads element instanceOffset + SV_InstanceID (= i)
                m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                m_context.DrawIndexedInstanced(mesh.indexCount, draw.count, mesh.firstIndex, mesh.baseVertex);
            } else {
                // One draw per object, each with its own element in the root constants: this keeps the
                // per-draw cost measurable. The material and buffer are the same for all of them, so only
                // instanceOffset is sent per draw.
                m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                for (uint32_t i = 0; i < draw.count; ++i) {
                    m_context.SetGraphicsRootConstant(gpu::kRootDrawConstants, draw.firstElement + i,
                                                      kInstanceOffsetConstant);
                    m_context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
                }
            }
        }
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

    void Renderer::CreatePipelines() {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};

        // A PSO fixes the pixel shader and the cull mode; a command list cannot change them between draws.
        // So each combination a material can ask for gets its own PSO, created here so no frame waits for one.
        struct Variant {
            Shading shading;
            bool doubleSided;
            std::wstring_view pixelShader;
            std::wstring_view name;
        };
        constexpr std::array kVariants{
            Variant{Shading::Lit, false, L"ps_light.cso", L"Lit PSO"},
            Variant{Shading::Lit, true, L"ps_light.cso", L"Lit two-sided PSO"},
            Variant{Shading::Unlit, false, L"ps_unlit.cso", L"Unlit PSO"},
            Variant{Shading::Unlit, true, L"ps_unlit.cso", L"Unlit two-sided PSO"},
        };
        ShadingPipelines &meshPipelines = m_pipelinesByFormat[std::to_underlying(VertexFormat::Mesh)];
        for (const Variant &variant : kVariants) {
            meshPipelines[std::to_underlying(variant.shading)][variant.doubleSided ? 1 : 0] = m_pipelines.Get({
                .vertexShader = L"vs_mesh.cso",
                .pixelShader = variant.pixelShader,
                .inputLayout = kVertexLayout,
                .cull = variant.doubleSided ? rhi::CullMode::None : rhi::CullMode::Back,
                .renderTargetFormats = kBackBufferTargets,
                .depthFormat = rhi::DepthBuffer::kFormat,
                .name = variant.name,
            });
        }
    }

    rhi::PipelineHandle Renderer::PipelineFor(VertexFormat format, const MaterialInfo &material) const {
        const rhi::PipelineHandle pipeline =
            m_pipelinesByFormat[std::to_underlying(format)][std::to_underlying(material.shading)]
                               [material.doubleSided ? 1 : 0];
        ENGINE_ASSERT_MSG(pipeline.IsValid(), "No PSO for vertex format {}, shading {}", std::to_underlying(format),
                          std::to_underlying(material.shading));
        return pipeline;
    }
} // namespace engine::render