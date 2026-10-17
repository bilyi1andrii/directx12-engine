#include "render/Renderer.h"

#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "rhi/UploadRing.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace engine::render {
    using core::ThrowIfFailed;

    namespace {
        // Compiled shader bytecode (DXIL), written by the build to <exe dir>/shaders/<name>.cso
        std::vector<std::byte> LoadShader(std::wstring_view fileName) {
            auto bytecode = core::ReadBinaryFile(core::ExecutableDirectory() / L"shaders" / fileName);
            if (!bytecode) {
                throw std::runtime_error(std::format("Cannot load shader {}", bytecode.error()));
            }
            return std::move(*bytecode);
        }

        // Matches vs_basic.hlsl's VSInput
        struct VertexPosColor {
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT3 color;
        };

        // The cube of 3dgep lesson 2: corners at +-1, each corner's colour from its position
        constexpr std::array<VertexPosColor, 8> kCubeVertices{{
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}}, // 0
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},  // 1
            {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}},   // 2
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},  // 3
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},  // 4
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},   // 5
            {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},    // 6
            {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}},   // 7
        }};

        // Two triangles per face, clockwise when seen from outside (the default front face)
        constexpr std::array<uint16_t, 36> kCubeIndices{
            0, 1, 2, 0, 2, 3, // -Z
            4, 6, 5, 4, 7, 6, // +Z
            4, 5, 1, 4, 1, 0, // -X
            3, 2, 6, 3, 6, 7, // +X
            1, 5, 6, 1, 6, 2, // +Y
            4, 0, 3, 4, 3, 7, // -Y
        };

        // The shader's view of FrameView: matrices transposed (DirectXMath uses row vectors, HLSL reads
        // column-major by default, so shaders compute mul(v, M)) plus the combined view-projection
        gpu::FrameConstants MakeFrameConstants(const FrameView &view) {
            using namespace DirectX;

            const XMMATRIX viewMatrix = XMLoadFloat4x4(&view.view);
            const XMMATRIX projection = XMLoadFloat4x4(&view.projection);

            gpu::FrameConstants constants{};
            XMStoreFloat4x4(&constants.view, XMMatrixTranspose(viewMatrix));
            XMStoreFloat4x4(&constants.projection, XMMatrixTranspose(projection));
            XMStoreFloat4x4(&constants.viewProjection, XMMatrixTranspose(viewMatrix * projection));
            constants.cameraPosition = view.cameraPosition;
            return constants;
        }
    } // namespace

    Renderer::Renderer(HWND hwnd, uint32_t width, uint32_t height, rhi::ValidationStats &validation)
        : m_device(validation), m_allocator(m_device),
          m_context(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list"),
          m_queue(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue"),
          m_rtvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap"),
          m_dsvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, L"DSV heap"),
          m_swapChain(m_device, m_queue, m_rtvAllocator, hwnd, width, height),
          m_depth(m_device.Native(), m_allocator, m_dsvAllocator, width, height, L"Depth buffer") {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), m_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  std::format(L"Frame {}", i));
        }
        // The global root signature is defined in HLSL (shaders/common/root_signature.hlsli) and compiled by
        // DXC into its own file, already validated and serialized: the device only deserializes it
        const std::vector<std::byte> rootSignature = LoadShader(L"rs_global.cso");
        ThrowIfFailed(m_device.Native()->CreateRootSignature(0, rootSignature.data(), rootSignature.size(),
                                                             IID_PPV_ARGS(&m_rootSignature)));
        ThrowIfFailed(m_rootSignature->SetName(L"Global root signature"));
        CreatePipelineState();
        UploadCube();
    }

    Renderer::~Renderer() {
        // The destructor body runs before any member is destroyed. Members declared after the
        // swap chain (depth buffer, PSO, meshes) are released before its flush, so wait here.
        try {
            m_queue.Flush();
        } catch (const std::exception &e) {
            core::LogError("Renderer: flush at destruction failed: {}", e.what());
        }
    }

    void Renderer::BeginFrame(uint32_t width, uint32_t height) {
        // This slot was last used kFramesInFlight frames ago; wait until the GPU has finished that
        // frame before reusing its allocator
        rhi::FrameContext &frame = CurrentFrame();
        frame.WaitAndReset(m_queue);

        // After WaitAndReset: objects a resize defers into this slot must survive until this
        // frame's fence, not be dropped by the reset
        if (m_swapChain.Resize(width, height)) {
            m_depth.Resize(m_swapChain.Width(), m_swapChain.Height(), frame);
        }

        m_context.Begin(frame.CommandAllocator());
    }

    void Renderer::Render(const FrameView &view) {
        ID3D12Resource *backBuffer = m_swapChain.CurrentBackBuffer();
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.CurrentRtv();

        constexpr std::array clearColor{0.4f, 0.6f, 0.9f, 1.0f};
        m_context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_context.ClearRenderTarget(rtv, clearColor);
        // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
        m_context.ClearDepth(m_depth.Dsv(), rhi::DepthBuffer::kClearDepth);

        m_context.SetRenderTarget(rtv, m_depth.Dsv());
        m_context.SetViewportAndScissor(m_swapChain.Width(), m_swapChain.Height());
        m_context.SetGraphicsRootSignature(m_rootSignature.Get());
        m_context.SetPipelineState(m_pipelineState.Get());

        // Per-frame constants go into this frame slot's upload ring: the GPU reads them while the CPU
        // already records the next frame into the other slot's ring
        const gpu::FrameConstants frameConstants = MakeFrameConstants(view);
        const rhi::UploadAllocation frameCb =
            CurrentFrame().Upload().Allocate(sizeof(frameConstants), rhi::kConstantBufferAlignment);
        std::memcpy(frameCb.cpu, &frameConstants, sizeof(frameConstants));
        m_context.SetGraphicsRootConstantBufferView(gpu::kRootFrameConstants, frameCb.gpu);

        m_context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context.SetVertexBuffer(m_cubeVertices.VertexBufferView(sizeof(VertexPosColor)));
        m_context.SetIndexBuffer(m_cubeIndices.IndexBufferView(DXGI_FORMAT_R16_UINT));
        m_context.DrawIndexed(static_cast<uint32_t>(kCubeIndices.size()));
    }

    void Renderer::EndFrame(bool vsync) {
        m_context.Transition(m_swapChain.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_PRESENT);
        m_context.Close();

        // Submit, present, and remember which fence value marks the end of this frame's GPU work
        m_queue.ExecuteCommandList(m_context.Native());
        m_swapChain.Present(vsync);
        CurrentFrame().SetFenceValue(m_queue.Signal());
        // Logs this frame's DXGI messages (Present, ResizeBuffers) next to the frame that caused them
        m_device.DrainDxgiMessages();
        ++m_frameNumber;
    }

    void Renderer::UploadCube() {
        // Recorded into the slot frame 0 uses: frame 0's BeginFrame waits for this fence value, then
        // resets the allocator and releases the upload buffers. No extra flush needed.
        ENGINE_ASSERT(m_frameNumber == 0);
        rhi::FrameContext &frame = CurrentFrame();
        m_context.Begin(frame.CommandAllocator());

        m_cubeVertices =
            rhi::Buffer::CreateWithData(m_allocator, m_context, frame, std::as_bytes(std::span(kCubeVertices)),
                                        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"Cube/VB");
        m_cubeIndices =
            rhi::Buffer::CreateWithData(m_allocator, m_context, frame, std::as_bytes(std::span(kCubeIndices)),
                                        D3D12_RESOURCE_STATE_INDEX_BUFFER, L"Cube/IB");

        // Must be submitted and signalled before the first BeginFrame: otherwise WaitAndReset would
        // reset an allocator the GPU is still reading
        m_context.Close();
        m_queue.ExecuteCommandList(m_context.Native());
        frame.SetFenceValue(m_queue.Signal());
    }

    void Renderer::CreatePipelineState() {
        const std::vector<std::byte> vertexShader = LoadShader(L"vs_basic.cso");
        const std::vector<std::byte> pixelShader = LoadShader(L"ps_basic.cso");

        // How the input assembler reads one vertex from the vertex buffer: must match VertexPosColor
        // on the CPU and VSInput in vs_basic.hlsl (semantic names, formats, byte offsets)
        const std::array inputLayout{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(VertexPosColor, position),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "COLOR",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(VertexPosColor, color),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
        };

        // A pipeline state object (PSO) bakes shaders and all fixed-function state into one object
        // up front, so the driver compiles everything once instead of at draw time. D3D12 has no
        // separate RSSetState/OMSetBlendState: changing any of this means switching PSOs.
        // The CD3DX12 defaults: no blending; solid fill, back faces culled, clockwise = front;
        // depth test LESS with depth writes on.
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
            .pRootSignature = m_rootSignature.Get(),
            .VS = {.pShaderBytecode = vertexShader.data(), .BytecodeLength = vertexShader.size()},
            .PS = {.pShaderBytecode = pixelShader.data(), .BytecodeLength = pixelShader.size()},
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = D3D12_DEFAULT_SAMPLE_MASK, // all samples
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .InputLayout = {.pInputElementDescs = inputLayout.data(),
                            .NumElements = static_cast<UINT>(inputLayout.size())},
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = {rhi::SwapChain::kFormat},
            // Must match the depth buffer; checked against the bound DSV when drawing
            .DSVFormat = rhi::DepthBuffer::kFormat,
            .SampleDesc = {.Count = 1, .Quality = 0},
        };

        ThrowIfFailed(m_device.Native()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipelineState)));
        ThrowIfFailed(m_pipelineState->SetName(L"Cube PSO"));
    }
} // namespace engine::render