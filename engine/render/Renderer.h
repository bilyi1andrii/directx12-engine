#ifndef D3D12_ENGINE_RENDERER_H
#define D3D12_ENGINE_RENDERER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

#include "renderdata/FrameView.h"
#include "renderdata/GpuUploader.h"
#include "renderdata/Handles.h"
#include "renderdata/InstanceChunks.h"
#include "renderdata/Lights.h"
#include "renderdata/Material.h"
#include "renderdata/MeshData.h"
#include "renderdata/RenderList.h"
#include "renderdata/RendererSettings.h"
#include "renderdata/TextureData.h"

// ImGui's per-frame draw data (ui::DebugUI::Render); opaque here
struct ImDrawData;

namespace engine::render {
    class UiRenderer;

    // Owns the GPU objects of the window's frame loop and records one frame at a time.
    //
    // Per frame: BeginFrame(width, height, settings) -> Render(view, lights, list, staticLists) -> [RenderUi(drawData)]
    // -> EndFrame(), each once and in this order (asserted). Any of them can throw core::HResultError. If the GPU
    // was lost, the destructor logs why (and the DRED info in Debug builds).
    //
    // Not copyable or movable (the swap chain and descriptors keep references to members).
    // Asset loading uses it through GpuUploader.
    class Renderer final : public GpuUploader {
    public:
        // `validation` collects debug-layer errors and leaks (Debug builds) and must outlive the renderer:
        // the device writes the live-object count when it is destroyed, after everything else
        Renderer(HWND hwnd, uint32_t width, uint32_t height, rhi::ValidationStats &validation);
        // Waits for the GPU before any member is released
        ~Renderer() override;

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
        // and lit by `lights`
        void Render(const FrameView &view, const LightSet &lights, const RenderList &list,
                    std::span<const StaticListHandle> staticLists = {});

        // Uploads the objects of `list` once into GPU memory. Pass the handle to Render
        // every frame to draw them; `list` can be cleared afterwards. Lives as long as the renderer.
        [[nodiscard]] StaticListHandle CreateStaticList(const RenderList &list, std::wstring_view name);

        // Uploads a model's geometry: one vertex buffer and one 32-bit index buffer shared by all its submeshes.
        // Returns one MeshHandle per submesh, in submesh order, so MeshNode::submesh indexes the result. The meshes
        // can be drawn in the same frame and live as long as the renderer.
        [[nodiscard]] std::vector<MeshHandle> CreateMesh(const MeshData &mesh, std::wstring_view name) override;

        // Creates a texture from a copy of `data`. It can be used in the same frame and lives as long as the renderer.
        [[nodiscard]] TextureHandle CreateTexture(const TextureData &data, std::wstring_view name) override;

        // 1x1 grey fallback for a texture that failed to load
        [[nodiscard]] TextureHandle UnloadedTexture() const noexcept override { return m_unloadedTexture; }

        // Adds `material` to the material table. It can be drawn in the same frame and lives as long as the
        // renderer. Costs no GPU memory: the table holds the albedo's index and picks the PSO.
        [[nodiscard]] MaterialHandle CreateMaterial(const MaterialInfo &material) override;

        // Starts the debug UI's renderer (ImGui's DirectX 12 backend). Call once, after the ImGui context exists
        // (ui::DebugUI); the context must outlive the renderer. Apps without UI (the bench) never call it.
        void EnableUi();

        // Draws the UI on top of the frame: after Render, before EndFrame, at most once per frame. Needs EnableUi.
        void RenderUi(ImDrawData *drawData);

        // Submits the frame, presents it (with this frame's RendererSettings::vsync) and remembers the fence value
        // that marks its end
        void EndFrame();

        [[nodiscard]] bool TearingSupported() const noexcept { return m_device.TearingSupported(); }
        // For benchmark results
        [[nodiscard]] const std::string &AdapterName() const noexcept { return m_device.AdapterName(); }
        [[nodiscard]] const std::string &DriverVersion() const noexcept { return m_device.DriverVersion(); }

    private:
        // Every vertex layout the renderer draws, each with its own PSO
        enum class VertexFormat : uint8_t {
            Mesh, // renderdata/Vertex.h (vs_mesh)
            Count,
        };

        // The last frame call made, for the asserts on their order
        enum class FrameStage : uint8_t {
            Idle, // before BeginFrame, or after EndFrame
            Begun,
            Rendered,
            UiRendered,
        };

        // A vertex buffer and an index buffer shared by all meshes of one model
        struct GpuGeometry {
            rhi::Buffer vertices;
            rhi::Buffer indices;
            uint32_t vertexStride = 0;
            DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
            VertexFormat vertexFormat = VertexFormat::Mesh; // one layout for every mesh in these buffers
        };

        // One drawable part of a geometry: a range of its index buffer. MeshHandle::index points into m_meshes.
        struct GpuMesh {
            uint32_t geometry = 0; // into m_geometries
            uint32_t firstIndex = 0;
            uint32_t indexCount = 0;
            int32_t baseVertex = 0; // added by the GPU to every index of this mesh
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

        [[nodiscard]] const MaterialInfo &Material(MaterialHandle handle) const;

        [[nodiscard]] const GpuStaticList &StaticList(StaticListHandle handle) const;

        // Takes ownership of `geometry`; returns its index in m_geometries
        [[nodiscard]] uint32_t AddGeometry(GpuGeometry geometry);
        // Registers a range of a geometry as a mesh
        [[nodiscard]] MeshHandle AddMesh(const GpuMesh &mesh);

        void CreatePipelines();

        [[nodiscard]] rhi::PipelineHandle PipelineFor(VertexFormat format, const MaterialInfo &material) const;

        // Makes sure an upload batch is open for `bytes` more staging memory. Submits the open batch first if it
        // would grow past 64 MB, so staging memory stays bounded while large models and textures load.
        void PrepareUpload(uint64_t bytes);

        // Creates the grey fallback texture (constructor only)
        void CreateFallbackTexture();

        // A 1x1 texture of one colour (sRGB values, as in an image file)
        [[nodiscard]] TextureHandle CreateSolidColorTexture(uint8_t red, uint8_t green, uint8_t blue,
                                                            std::wstring_view name);

        // Shutdown only: logs why the device was removed (if it was), waits for the GPU, then destroys every
        // texture, mesh buffer and static list
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
        // Back buffers and other render targets; 64 slots cost a few KB of CPU memory
        rhi::DescriptorAllocator m_rtvAllocator;
        // The main depth buffer, with room for shadow maps
        rhi::DescriptorAllocator m_dsvAllocator;
        // After the queue and the allocator: its destructor flushes the queue and returns its RTVs
        rhi::SwapChain m_swapChain;
        // Same size as the back buffers; after the DSV allocator, so its DSV is returned first
        rhi::DepthBuffer m_depth;

        // The global root signature and every PSO
        rhi::PipelineCache m_pipelines;
        // One PSO per vertex format, shading and culling (index 1: two-sided, no culling). Filled by
        // CreatePipelines; read through PipelineFor.
        using ShadingPipelines = std::array<std::array<rhi::PipelineHandle, 2>, kShadingCount>;
        std::array<ShadingPipelines, std::to_underlying(VertexFormat::Count)> m_pipelinesByFormat;
        std::vector<GpuGeometry> m_geometries;
        std::vector<GpuMesh> m_meshes;
        // All textures; a TextureHandle is a bindless index, not a position in this list
        std::vector<rhi::Texture> m_textures;
        TextureHandle m_unloadedTexture;
        // MaterialHandle::index points into it. Entry 0 is the grey texture, lit, so a default handle can be drawn.
        std::vector<MaterialInfo> m_materials;

        // Released after the destructor's flush, before the allocator
        std::vector<GpuStaticList> m_staticLists;

        // The UI's renderer; null until EnableUi. Released after the destructor's flush, before the frames and the
        // bindless heap: its shutdown hands the font texture's descriptor to a frame's deferred frees.
        std::unique_ptr<UiRenderer> m_ui;

        // Rebuilt for every list; members, so their memory is reused
        std::vector<InstanceRange> m_instanceRanges;
        std::vector<InstanceDraw> m_dynamicDraws;

        RendererSettings m_settings; // this frame's copy, taken in BeginFrame
        uint64_t m_frameNumber = 0;
        FrameStage m_frameStage = FrameStage::Idle;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERER_H