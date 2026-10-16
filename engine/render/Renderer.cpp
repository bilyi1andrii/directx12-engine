#include "render/Renderer.h"

#include <array>
#include <exception>
#include <format>

#include "core/Log.h"

namespace engine::render {
    Renderer::Renderer(HWND hwnd, uint32_t width, uint32_t height)
        : m_context(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list"),
          m_queue(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue"),
          m_rtvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap"),
          m_swapChain(m_device, m_queue, m_rtvAllocator, hwnd, width, height) {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, std::format(L"Frame {}", i));
        }
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
        m_swapChain.Resize(width, height);

        m_context.Begin(frame.Allocator());
    }

    void Renderer::Render() {
        ID3D12Resource *backBuffer = m_swapChain.CurrentBackBuffer();

        constexpr std::array clearColor{0.4f, 0.6f, 0.9f, 1.0f};
        m_context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_context.ClearRenderTarget(m_swapChain.CurrentRtv(), clearColor);
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
} // namespace engine::render