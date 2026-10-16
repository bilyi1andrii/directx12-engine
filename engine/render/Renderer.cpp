#include "render/Renderer.h"

#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <span>

#include <DirectXMath.h>

#include "core/Assert.h"
#include "core/Log.h"

namespace engine::render {
    namespace {
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
    } // namespace

    Renderer::Renderer(HWND hwnd, uint32_t width, uint32_t height)
        : m_context(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list"),
          m_queue(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue"),
          m_rtvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap"),
          m_dsvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, L"DSV heap"),
          m_swapChain(m_device, m_queue, m_rtvAllocator, hwnd, width, height),
          m_depth(m_device.Native(), m_dsvAllocator, width, height, L"Depth buffer") {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, std::format(L"Frame {}", i));
        }
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

        m_context.Begin(frame.Allocator());
    }

    void Renderer::Render() {
        ID3D12Resource *backBuffer = m_swapChain.CurrentBackBuffer();

        constexpr std::array clearColor{0.4f, 0.6f, 0.9f, 1.0f};
        m_context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_context.ClearRenderTarget(m_swapChain.CurrentRtv(), clearColor);
        // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
        m_context.ClearDepth(m_depth.Dsv(), rhi::DepthBuffer::kClearDepth);
    }

    void Renderer::EndFrame(bool vsync) {
        m_context.Transition(m_swapChain.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_PRESENT);
        m_context.Close();

        // Submit, present, and remember which fence value marks the end of this frame's GPU work
        m_queue.ExecuteCommandList(m_context.Native());
        m_swapChain.Present(vsync);
        CurrentFrame().SetFenceValue(m_queue.Signal());
        ++m_frameNumber;
    }

    void Renderer::UploadCube() {
        // Recorded into the slot frame 0 uses: frame 0's BeginFrame waits for this fence value, then
        // resets the allocator and releases the upload buffers. No extra flush needed.
        ENGINE_ASSERT(m_frameNumber == 0);
        rhi::FrameContext &frame = CurrentFrame();
        m_context.Begin(frame.Allocator());

        m_cubeVertices =
            rhi::Buffer::CreateWithData(m_device.Native(), m_context, frame, std::as_bytes(std::span(kCubeVertices)),
                                        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"Cube/VB");
        m_cubeIndices =
            rhi::Buffer::CreateWithData(m_device.Native(), m_context, frame, std::as_bytes(std::span(kCubeIndices)),
                                        D3D12_RESOURCE_STATE_INDEX_BUFFER, L"Cube/IB");

        // Must be submitted and signalled before the first BeginFrame: otherwise WaitAndReset would
        // reset an allocator the GPU is still reading
        m_context.Close();
        m_queue.ExecuteCommandList(m_context.Native());
        frame.SetFenceValue(m_queue.Signal());
    }
} // namespace engine::render