#ifndef D3D12_ENGINE_GPURESOURCES_H
#define D3D12_ENGINE_GPURESOURCES_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/Assert.h"
#include "rhi/Buffer.h"
#include "rhi/Texture.h"

#include "renderdata/GpuUploader.h"
#include "renderdata/Handles.h"
#include "renderdata/InstanceChunks.h"
#include "renderdata/Material.h"
#include "renderdata/MeshData.h"
#include "renderdata/RenderList.h"
#include "renderdata/TextureData.h"

namespace engine::rhi {
    class Allocator;
    class BindlessHeap;
    class CommandQueue;
    class FrameContext;
    class UploadContext;
} // namespace engine::rhi

namespace engine::render {
    // Every vertex layout the renderer draws; each pass has its own PSOs per layout
    enum class VertexFormat : uint8_t {
        Mesh, // renderdata/Vertex.h (vs_mesh)
        Count,
    };

    // A vertex buffer and an index buffer shared by all meshes of one model
    struct GpuGeometry {
        rhi::Buffer vertices;
        rhi::Buffer indices;
        uint32_t vertexStride = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        VertexFormat vertexFormat = VertexFormat::Mesh; // one layout for every mesh in these buffers
    };

    // One drawable part of a geometry: a range of its index buffer
    struct GpuMesh {
        uint32_t geometry = 0; // GpuResources::Geometry(geometry) holds its buffers
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t baseVertex = 0; // added by the GPU to every index of this mesh
    };

    // A static list in GPU memory: its instances, their view and its draws
    struct GpuStaticList {
        rhi::Buffer instances;            // empty if the list has no objects
        uint32_t instanceBufferIndex = 0; // bindless index of the view; only valid if `instances` is
        std::vector<InstanceDraw> draws;
    };

    // Every mesh, texture, material and static list the renderer can draw. Asset loading creates them through
    // GpuUploader; passes look them up by handle while they record.
    //
    // Everything lives as long as this object. Creation records copies into the open upload batch of
    // `upload` (or opens one); the owner submits it before the frame that first draws the new resources.
    // DestroyAll must run after a GPU flush and before destruction (asserted).
    //
    // Not copyable or movable: it keeps references to the allocator, the upload context, the queue and the heap,
    // which must outlive it.
    class GpuResources final : public GpuUploader {
    public:
        GpuResources(rhi::Allocator &allocator, rhi::UploadContext &upload, rhi::CommandQueue &queue,
                     rhi::BindlessHeap &bindlessHeap) noexcept;
        // Asserts that DestroyAll ran
        ~GpuResources() override;

        GpuResources(const GpuResources &) = delete;
        GpuResources &operator=(const GpuResources &) = delete;
        GpuResources(GpuResources &&) = delete;
        GpuResources &operator=(GpuResources &&) = delete;

        // Creates the grey fallback texture and material 0, so a default handle draws something. Call once, before
        // any other texture: the grey texture must get bindless index 0.
        void CreateDefaults();

        // Uploads a model's geometry: one vertex buffer and one 32-bit index buffer shared by all its submeshes.
        // Returns one MeshHandle per submesh, in submesh order, so MeshNode::submesh indexes the result. The meshes
        // can be drawn in the same frame.
        [[nodiscard]] std::vector<MeshHandle> CreateMesh(const MeshData &mesh, std::wstring_view name) override;

        // Creates a texture from a copy of `data`. It can be used in the same frame.
        [[nodiscard]] TextureHandle CreateTexture(const TextureData &data, std::wstring_view name) override;

        // 1x1 grey fallback for a texture that failed to load
        [[nodiscard]] TextureHandle UnloadedTexture() const noexcept override { return m_unloadedTexture; }

        // Adds `material` to the material table. The handle works in the same frame.
        // Costs no GPU memory: an entry is only the albedo's index and the choice of PSO.
        [[nodiscard]] MaterialHandle CreateMaterial(const MaterialInfo &material) override;

        // Uploads the objects of `list` once into GPU memory. Pass the handle to Renderer::Render every frame to
        // draw them; `list` can be cleared afterwards.
        [[nodiscard]] StaticListHandle CreateStaticList(const RenderList &list, std::wstring_view name);

        // Lookups for recording. Defined here so they inline: the non-instanced path calls them once per object.
        [[nodiscard]] const GpuMesh &Mesh(MeshHandle handle) const {
            ENGINE_ASSERT_MSG(handle.index < m_meshes.size(), "Mesh handle {} out of range ({} meshes)", handle.index,
                              m_meshes.size());
            return m_meshes[handle.index];
        }

        // `index` is GpuMesh::geometry
        [[nodiscard]] const GpuGeometry &Geometry(uint32_t index) const {
            ENGINE_ASSERT_MSG(index < m_geometries.size(), "Geometry {} out of range ({} geometries)", index,
                              m_geometries.size());
            return m_geometries[index];
        }

        [[nodiscard]] const MaterialInfo &Material(MaterialHandle handle) const {
            ENGINE_ASSERT_MSG(handle.index < m_materials.size(), "Material handle {} out of range ({} materials)",
                              handle.index, m_materials.size());
            return m_materials[handle.index];
        }

        [[nodiscard]] const GpuStaticList &StaticList(StaticListHandle handle) const {
            ENGINE_ASSERT_MSG(handle.index < m_staticLists.size(), "Static list handle {} out of range ({} lists)",
                              handle.index, m_staticLists.size());
            return m_staticLists[handle.index];
        }

        // Shutdown only, after the GPU is idle: hands every texture, mesh buffer and static list to `frame`, which
        // releases them when it is reset or destroyed. Handles are invalid afterwards.
        void DestroyAll(rhi::FrameContext &frame) noexcept;

    private:
        // Takes ownership of `geometry`; returns its index in m_geometries
        [[nodiscard]] uint32_t AddGeometry(GpuGeometry geometry);
        // Registers a range of a geometry as a mesh
        [[nodiscard]] MeshHandle AddMesh(const GpuMesh &mesh);

        // Makes sure an upload batch is open for `bytes` more staging memory. Submits the open batch first if it
        // would grow past 64 MB, so staging memory stays bounded while large models and textures load.
        void PrepareUpload(uint64_t bytes);

        // A 1x1 texture of one colour (sRGB values, as in an image file)
        [[nodiscard]] TextureHandle CreateSolidColorTexture(uint8_t red, uint8_t green, uint8_t blue,
                                                            std::wstring_view name);

        rhi::Allocator &m_allocator;
        rhi::UploadContext &m_upload;
        // Only for submitting a full upload batch (PrepareUpload)
        rhi::CommandQueue &m_queue;
        rhi::BindlessHeap &m_bindlessHeap;

        std::vector<GpuGeometry> m_geometries;
        // Indexed by MeshHandle::index
        std::vector<GpuMesh> m_meshes;
        // All textures; a TextureHandle is a bindless index, not a position in this list
        std::vector<rhi::Texture> m_textures;
        TextureHandle m_unloadedTexture;
        // Indexed by MaterialHandle::index. Entry 0 is the grey texture, lit, so a default handle draws something.
        std::vector<MaterialInfo> m_materials;
        // Indexed by StaticListHandle::index
        std::vector<GpuStaticList> m_staticLists;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_GPURESOURCES_H
