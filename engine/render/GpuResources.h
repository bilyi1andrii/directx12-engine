#ifndef D3D12_ENGINE_GPURESOURCES_H
#define D3D12_ENGINE_GPURESOURCES_H

// Every mesh, texture, material and static list the renderer can draw. Assets create them, the passes look them up
// by handle, and new ones are uploaded before the frame that first draws them.

#include <cstdint>
#include <string_view>
#include <vector>

#include <DirectXMath.h>

#include "rhi/Buffer.h"
#include "rhi/Texture.h"

#include "renderdata/GpuUploader.h"
#include "renderdata/Handles.h"
#include "renderdata/InstanceChunks.h"
#include "renderdata/Material.h"
#include "renderdata/MeshData.h"
#include "renderdata/RenderList.h"
#include "renderdata/TextureData.h"
#include "renderdata/TransparentSort.h"

namespace rhi {
    class Allocator;
    class BindlessHeap;
    class CommandQueue;
    class FrameContext;
    class UploadContext;
} // namespace rhi

namespace render {
    enum class VertexFormat : uint8_t {
        Mesh,
        Count,
    };

    struct GpuGeometry {
        rhi::Buffer vertices;
        rhi::Buffer indices;
        uint32_t vertexStride = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        VertexFormat vertexFormat = VertexFormat::Mesh;
    };

    struct GpuMesh {
        uint32_t geometry = 0;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t baseVertex = 0;
        DirectX::XMFLOAT3 center{};
    };

    struct StaticTransparentObject {
        BatchKey key;
        uint32_t element = 0;
        DirectX::XMFLOAT3 position{};
    };

    struct GpuStaticList {
        rhi::Buffer instances;
        uint32_t instanceBufferIndex = 0; // bindless
        std::vector<InstanceDraw> draws;
        std::vector<StaticTransparentObject> transparent;
    };

    class GpuResources final : public GpuUploader {
        friend class Renderer;

    public:
        GpuResources(rhi::Allocator &allocator, rhi::UploadContext &upload, rhi::CommandQueue &queue,
                     rhi::BindlessHeap &bindlessHeap) noexcept;
        ~GpuResources() override;

        GpuResources(const GpuResources &) = delete;
        GpuResources &operator=(const GpuResources &) = delete;
        GpuResources(GpuResources &&) = delete;
        GpuResources &operator=(GpuResources &&) = delete;

        [[nodiscard]] std::vector<MeshHandle> CreateMesh(const MeshData &mesh, std::wstring_view name) override;

        [[nodiscard]] TextureHandle CreateTexture(const TextureData &data, std::wstring_view name) override;

        [[nodiscard]] TextureHandle UnloadedTexture() const noexcept override { return m_unloadedTexture; }

        [[nodiscard]] MaterialHandle CreateMaterial(const MaterialInfo &material) override;

        [[nodiscard]] StaticListHandle CreateStaticList(const RenderList &list, std::wstring_view name);

        [[nodiscard]] const GpuMesh &Mesh(MeshHandle handle) const { return m_meshes[handle.index]; }

        [[nodiscard]] const GpuGeometry &Geometry(uint32_t index) const { return m_geometries[index]; }

        [[nodiscard]] const MaterialInfo &Material(MaterialHandle handle) const { return m_materials[handle.index]; }

        [[nodiscard]] const GpuStaticList &StaticList(StaticListHandle handle) const {
            return m_staticLists[handle.index];
        }

    private:
        void CreateDefaults();

        void DestroyAll(rhi::FrameContext &frame) noexcept;

        [[nodiscard]] uint32_t AddGeometry(GpuGeometry geometry);
        [[nodiscard]] MeshHandle AddMesh(const GpuMesh &mesh);

        void PrepareUpload(uint64_t bytes);

        [[nodiscard]] TextureHandle CreateSolidColorTexture(uint8_t red, uint8_t green, uint8_t blue,
                                                            std::wstring_view name);

        rhi::Allocator &m_allocator;
        rhi::UploadContext &m_upload;
        rhi::CommandQueue &m_queue;
        rhi::BindlessHeap &m_bindlessHeap;

        std::vector<GpuGeometry> m_geometries;
        std::vector<GpuMesh> m_meshes;
        std::vector<rhi::Texture> m_textures;
        TextureHandle m_unloadedTexture;
        std::vector<MaterialInfo> m_materials;
        std::vector<GpuStaticList> m_staticLists;
    };
} // namespace render

#endif // D3D12_ENGINE_GPURESOURCES_H
