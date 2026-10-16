#ifndef D3D12_ENGINE_RENDERER_H
#define D3D12_ENGINE_RENDERER_H

#include <cstdint>
#include <vector>

#include "rhi/Buffer.h"
#include "rhi/CommandContext.h"
#include "rhi/CommandQueue.h"
#include "rhi/DepthBuffer.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/Device.h"
#include "rhi/FrameContext.h"
#include "rhi/SwapChain.h"

namespace engine::render {
    // Owns the GPU objects of the window's frame loop and records one frame at a time.
    //
    // Per frame: BeginFrame(width, height) -> Render() -> EndFrame(vsync).
    // Any of the three can throw core::HResultError; after DXGI_ERROR_DEVICE_REMOVED call
    // LogDeviceRemovedInfo() before rethrowing.
    //
    // Not copyable or movable (the swap chain and descriptors keep references to members).
    class Renderer {
    public:
        Renderer(HWND hwnd, uint32_t width, uint32_t height);
        // Waits for the GPU before any member is released
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        // Waits until this frame's slot is free, matches the swap chain to the window's client size
        // (no-op when unchanged) and starts recording
        void BeginFrame(uint32_t width, uint32_t height);

        // Records the frame's commands. timeSeconds: time since start, animates the cube
        void Render(double timeSeconds);

        // Submits the frame, presents it and remembers the fence value that marks its end
        void EndFrame(bool vsync);

        // If the device was removed, logs the reason (and DRED info in Debug builds)
        void LogDeviceRemovedInfo() const { m_device.LogDeviceRemovedInfo(); }

        [[nodiscard]] bool TearingSupported() const noexcept { return m_device.TearingSupported(); }

    private:
        [[nodiscard]] rhi::FrameContext &CurrentFrame() { return m_frames[m_frameNumber % rhi::kFramesInFlight]; }

        // Create the cube's root signature and pipeline state (called once, from the constructor).
        // Temporary: step 2 replaces them with the global root signature and PipelineCache.
        void CreateRootSignature();
        void CreatePipelineState();

        // Uploads the cube's vertex and index buffers (called once, from the constructor)
        void UploadCube();

        // Members are destroyed in reverse order. The device is first, so it goes last and its
        // live-object report sees everything else released. The frames and the command list come
        // before the queue: the queue's destructor waits for the GPU before they are released.
        rhi::Device m_device;
        std::vector<rhi::FrameContext> m_frames;
        rhi::CommandContext m_context;
        rhi::CommandQueue m_queue;
        // Back buffers plus the render targets of later steps; 64 slots cost a few KB of CPU memory
        rhi::DescriptorAllocator m_rtvAllocator;
        // The main depth buffer plus the shadow maps of step 4
        rhi::DescriptorAllocator m_dsvAllocator;
        // After the queue and the allocator: its destructor flushes the queue and returns its RTVs
        rhi::SwapChain m_swapChain;
        // Same size as the back buffers; after the DSV allocator, so its DSV is returned first
        rhi::DepthBuffer m_depth;

        rhi::ComPtr<ID3D12RootSignature> m_rootSignature;
        rhi::ComPtr<ID3D12PipelineState> m_pipelineState;
        rhi::Buffer m_cubeVertices;
        rhi::Buffer m_cubeIndices;

        uint64_t m_frameNumber = 0;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERER_H