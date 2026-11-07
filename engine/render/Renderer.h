#ifndef D3D12_ENGINE_RENDERER_H
#define D3D12_ENGINE_RENDERER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <DirectXMath.h>

#include "rhi/Allocator.h"
#include "rhi/BindlessHeap.h"
#include "rhi/Buffer.h"
#include "rhi/CommandContext.h"
#include "rhi/CommandQueue.h"
#include "rhi/DepthBuffer.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/Device.h"
#include "rhi/FrameContext.h"
#include "rhi/PipelineCache.h"
#include "rhi/SwapChain.h"
#include "rhi/Texture.h"
#include "rhi/UploadContext.h"
#include "rhi/UploadRing.h"
#include "rhi/ValidationStats.h"

#include "renderdata/Handles.h"
#include "renderdata/InstanceChunks.h"
#include "renderdata/RenderList.h"
#include "renderdata/RendererSettings.h"
#include "renderdata/TextureData.h"

namespace engine::render {
    struct FrameView {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT3 cameraPosition; // world space
    };

    // Owns the GPU objects of the window's frame loop and records one frame at a time.
    //
    // Per frame: BeginFrame(width, height, settings) -> Render(view, list, staticLists) -> EndFrame(vsync).
    // Any of the three can throw core::HResultError; after DXGI_ERROR_DEVICE_REMOVED call
    // LogDeviceRemovedInfo() before rethrowing.
    //
    // Not copyable or movable (the swap chain and descriptors keep references to members).
    class Renderer {
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
        void Render(const FrameView &view, const RenderList &list, std::span<const StaticListHandle> staticLists = {});

        // Uploads the objects of `list` once into GPU memory (DX11's static buckets). Pass the handle to Render
        // every frame to draw them; `list` can be cleared afterwards. Lives as long as the renderer.
        [[nodiscard]] StaticListHandle CreateStaticList(const RenderList &list, std::wstring_view name);

        // The built-in cubes (corners at +-1), until meshes come from AssetCache: CubeMesh has vertex colours,
        // TexturedCubeMesh shows its material's texture
        [[nodiscard]] MeshHandle CubeMesh() const noexcept { return m_cubeMesh; }
        [[nodiscard]] MeshHandle TexturedCubeMesh() const noexcept { return m_texturedCubeMesh; }

        // Creates a texture from a copy of `data`. It can be used in the same frame and lives as long as the renderer.
        [[nodiscard]] TextureHandle CreateTexture(const TextureData &data, std::wstring_view name);

        // 1x1 fallback textures (DX11 Color.h): grey for a texture that failed to load, red for an unhandled type
        [[nodiscard]] TextureHandle UnloadedTexture() const noexcept { return m_unloadedTexture; }
        [[nodiscard]] TextureHandle UnhandledTexture() const noexcept { return m_unhandledTexture; }

        // Submits the frame, presents it and remembers the fence value that marks its end
        void EndFrame(bool vsync);

        // If the device was removed, logs the reason (and DRED info in Debug builds)
        void LogDeviceRemovedInfo() const { m_device.LogDeviceRemovedInfo(); }

        [[nodiscard]] bool TearingSupported() const noexcept { return m_device.TearingSupported(); }

    private:
        enum class VertexFormat : uint8_t {
            PositionColor, // the built-in vertex-colour cube (vs_basic)
            Mesh,          // renderdata/Vertex.h (vs_mesh)
            Count,
        };

        // A mesh's GPU buffers and how to draw them. MeshHandle::index points into m_meshes.
        struct GpuMesh {
            rhi::Buffer vertices;
            rhi::Buffer indices;
            uint32_t vertexStride = 0;
            DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
            uint32_t indexCount = 0;
            VertexFormat vertexFormat = VertexFormat::Mesh;
        };

        // One instanced draw: `count` objects with the same key, from element `firstElement` of the instance
        // buffer at bindless index `instanceBufferIndex`. Static and dynamic objects differ only in that buffer.
        struct InstanceDraw {
            BatchKey key;
            uint32_t instanceBufferIndex = 0;
            uint32_t firstElement = 0;
            uint32_t count = 0;
        };

        // A static list in GPU memory: its instances, their view and its draws. StaticListHandle::index points
        // into m_staticLists.
        struct GpuStaticList {
            rhi::Buffer instances;            // empty if the list has no objects
            uint32_t instanceBufferIndex = 0; // bindless index of the view; only valid if `instances` is
            std::vector<InstanceDraw> draws;
        };

        // Where WriteInstances writes one chunk, and the view its draws read
        struct InstanceChunk {
            rhi::UploadAllocation memory;
            uint32_t instanceBufferIndex = 0;
        };

        [[nodiscard]] rhi::FrameContext &CurrentFrame() { return m_frames[m_frameNumber % rhi::kFramesInFlight]; }

        [[nodiscard]] const GpuMesh &Mesh(MeshHandle handle) const;

        [[nodiscard]] const GpuStaticList &StaticList(StaticListHandle handle) const;

        void CreatePipelines();

        [[nodiscard]] rhi::PipelineHandle PipelineFor(VertexFormat format, MaterialHandle material) const;

        // Uploads the vertex and index buffers of both cubes (constructor only)
        void UploadCubes();

        // Creates the grey and red fallback textures (constructor only)
        void CreateFallbackTextures();

        // A 1x1 sRGB texture of one colour
        [[nodiscard]] TextureHandle CreateSolidColorTexture(uint8_t red, uint8_t green, uint8_t blue,
                                                            std::wstring_view name);

        // Shutdown only: waits for the GPU, then destroys every texture and frees the static lists' views
        void DestroyResources() noexcept;

        // Shared by dynamic and static instances: splits `list` into chunks of at most `chunkCapacity` objects,
        // writes their transposed world matrices and appends their draws to `draws`. beginChunk(elementCount)
        // is called once per chunk and returns where to write it.
        template <typename BeginChunk>
        void WriteInstances(const RenderList &list, uint32_t chunkCapacity, BeginChunk &&beginChunk,
                            std::vector<InstanceDraw> &draws);

        // Writes the objects of `list` into this frame's upload ring and fills m_dynamicDraws
        void UploadDynamicInstances(const RenderList &list);

        // Records `draws` on the current draw path: one instanced draw each, or one draw per object
        void RecordDraws(std::span<const InstanceDraw> draws);

        // Members are destroyed in reverse order. The device is first, so it goes last and its
        // live-object report sees everything else released. The resource allocator is next: every
        // buffer, texture and deferred release (in the frames) must be gone before it. The frames, the
        // upload context and the command list come before the queue: the queue's destructor waits for
        // the GPU before they are released, even when the constructor throws.
        rhi::Device m_device;
        rhi::Allocator m_allocator;
        // Before the frames: their deferred frees return indices to it, also when they are destroyed
        rhi::BindlessHeap m_bindlessHeap;
        std::vector<rhi::FrameContext> m_frames;
        // Uploads of meshes and textures
        rhi::UploadContext m_upload;
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

        // The global root signature and every PSO
        rhi::PipelineCache m_pipelines;
        // Filled by CreatePipelines; indexed by VertexFormat
        std::array<rhi::PipelineHandle, static_cast<size_t>(VertexFormat::Count)> m_pipelineByFormat;
        std::vector<GpuMesh> m_meshes;
        MeshHandle m_cubeMesh;
        MeshHandle m_texturedCubeMesh;
        // All textures; a TextureHandle is a bindless index, not a position in this list
        std::vector<rhi::Texture> m_textures;
        TextureHandle m_unloadedTexture;
        TextureHandle m_unhandledTexture;

        // Released after the destructor's flush, before the allocator
        std::vector<GpuStaticList> m_staticLists;

        // Rebuilt for every list; members, so their memory is reused
        std::vector<InstanceRange> m_instanceRanges;
        std::vector<InstanceDraw> m_dynamicDraws;

        RendererSettings m_settings; // this frame's copy, taken in BeginFrame
        uint64_t m_frameNumber = 0;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERER_H