#include "render/GpuResources.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Assert.h"
#include "render/InstanceUpload.h"
#include "renderdata/Model.h"
#include "renderdata/TransparentSort.h"
#include "renderdata/Vertex.h"
#include "rhi/Allocator.h"
#include "rhi/BindlessHeap.h"
#include "rhi/CommandQueue.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadContext.h"

namespace render {
    GpuResources::GpuResources(rhi::NativeDevice *device, rhi::Allocator &allocator, rhi::UploadContext &upload,
                               rhi::CommandQueue &queue, rhi::BindlessHeap &bindlessHeap) noexcept
        : m_device(device), m_allocator(allocator), m_upload(upload), m_queue(queue), m_bindlessHeap(bindlessHeap) {}

    GpuResources::~GpuResources() {
        ENGINE_ASSERT_MSG(m_textures.empty() && m_geometries.empty() && m_staticLists.empty(),
                          "GpuResources destroyed without DestroyAll ({} textures, {} geometries, {} static lists)",
                          m_textures.size(), m_geometries.size(), m_staticLists.size());
    }

    void GpuResources::CreateDefaults() {
        m_unloadedTexture = CreateSolidColorTexture(100, 100, 100, L"Fallback/Unloaded (grey)");
        m_materials.push_back(MaterialInfo{.albedo = m_unloadedTexture}); // material 0, default handle draws grey
    }

    std::vector<MeshHandle> GpuResources::CreateMesh(const MeshData &mesh, std::wstring_view name) {
        PrepareUpload(std::span(mesh.vertices).size_bytes() + std::span(mesh.indices).size_bytes());
        const std::wstring prefix(name);
        const uint32_t geometry = AddGeometry({
            .vertices = rhi::Buffer::CreateWithData(m_allocator, m_upload, mesh.vertices,
                                                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, prefix + L"/VB"),
            .indices = rhi::Buffer::CreateWithData(m_allocator, m_upload, mesh.indices,
                                                   D3D12_RESOURCE_STATE_INDEX_BUFFER, prefix + L"/IB"),
            .vertexStride = sizeof(Vertex),
            .indexFormat = DXGI_FORMAT_R32_UINT,
            .vertexFormat = VertexFormat::Mesh,
        });

        std::vector<MeshHandle> handles;
        handles.reserve(mesh.submeshes.size());
        for (const SubmeshData &submesh : mesh.submeshes) {
            handles.push_back(AddMesh({
                .geometry = geometry,
                .firstIndex = submesh.firstIndex,
                .indexCount = submesh.indexCount,
                .baseVertex = submesh.baseVertex,
                .center = Center(SubmeshBounds(mesh, submesh)),
            }));
        }
        return handles;
    }

    TextureHandle GpuResources::CreateTexture(const TextureData &data, std::wstring_view name) {
        std::array<rhi::TextureMipData, D3D12_REQ_MIP_LEVELS> mips{};
        const std::span<const std::byte> pixels(data.pixels);
        for (size_t i = 0; i < data.mips.size(); ++i) {
            const TextureMip &mip = data.mips[i];
            mips[i] = {.width = mip.width,
                       .height = mip.height,
                       .rowPitch = mip.rowPitch,
                       .pixels = pixels.subspan(mip.offset, mip.slicePitch)};
        }

        PrepareUpload(pixels.size_bytes());
        rhi::Texture texture = rhi::Texture::Create(m_device, m_allocator, m_upload, m_bindlessHeap,
                                                    static_cast<DXGI_FORMAT>(data.dxgiFormat),
                                                    std::span(mips).first(data.mips.size()), name);
        const TextureHandle handle{.index = texture.BindlessIndex()}; // handle is bindless slot
        m_textures.push_back(std::move(texture));
        return handle;
    }

    MaterialHandle GpuResources::CreateMaterial(const MaterialInfo &material) {
        ENGINE_ASSERT_MSG(material.shading == Shading::Lit || material.alphaMode == AlphaMode::Opaque,
                          "Unlit material with alpha mode {}", std::to_underlying(material.alphaMode));
        ENGINE_ASSERT_MSG(material.alphaMode == AlphaMode::Opaque || material.opacityMap != m_unloadedTexture,
                          "Material with alpha mode {} has the grey texture as its opacity map: use a white one",
                          std::to_underlying(material.alphaMode));
        m_materials.push_back(material);
        return MaterialHandle{.index = static_cast<uint32_t>(m_materials.size() - 1)};
    }

    StaticListHandle GpuResources::CreateStaticList(const RenderList &list, std::wstring_view name) {
        // static objects never move, so sort points computed once
        std::vector<StaticTransparentObject> transparent;
        uint32_t element = 0;
        for (const BatchBucket &bucket : list.Buckets()) {
            if (Material(bucket.key.material).alphaMode == AlphaMode::Blend) {
                const DirectX::XMFLOAT3 &center = Mesh(bucket.key.mesh).center;
                for (uint32_t i = 0; i < bucket.worlds.size(); ++i) {
                    transparent.push_back(
                        {.key = bucket.key, .element = element + i, .position = SortPoint(bucket.worlds[i], center)});
                }
            }
            element += static_cast<uint32_t>(bucket.worlds.size());
        }

        // so push_back below can't throw after buffer exists
        m_staticLists.reserve(m_staticLists.size() + 1);
        std::optional<rhi::Buffer> instances;
        uint32_t instanceBufferIndex = 0;
        std::vector<InstanceDraw> draws;
        const uint64_t instanceCount = list.InstanceCount();
        if (instanceCount > 0) {
            PrepareUpload(instanceCount * kInstanceStride);
            const auto beginChunk = [&](uint32_t elementCount) {
                InstanceChunk chunk{};
                instances.emplace(
                    rhi::Buffer::CreateStaged(m_allocator, m_upload, uint64_t{elementCount} * kInstanceStride,
                                              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, name, chunk.memory));
                instanceBufferIndex = m_bindlessHeap.Allocate();
                m_bindlessHeap.CreateStructuredBufferSrv(instanceBufferIndex, instances->Native(), 0, elementCount,
                                                         kInstanceStride);
                chunk.instanceBufferIndex = instanceBufferIndex;
                return chunk;
            };
            std::vector<InstanceRange> ranges;
            // capacity is whole list, so one chunk
            WriteInstances(list, static_cast<uint32_t>(instanceCount), beginChunk, ranges, draws);
        }

        const StaticListHandle handle{.index = static_cast<uint32_t>(m_staticLists.size())};
        m_staticLists.push_back(GpuStaticList{
            .instances = instances ? std::move(*instances) : rhi::Buffer{},
            .instanceBufferIndex = instanceBufferIndex,
            .draws = std::move(draws),
            .transparent = std::move(transparent),
        });
        return handle;
    }

    void GpuResources::DestroyAll(rhi::FrameContext &frame) noexcept {
        for (rhi::Texture &texture : m_textures) {
            texture.Destroy(frame, m_bindlessHeap);
        }
        m_textures.clear();
        m_unloadedTexture = {};
        m_materials.clear();
        for (GpuGeometry &geometry : m_geometries) {
            geometry.vertices.Destroy(frame);
            geometry.indices.Destroy(frame);
        }
        m_geometries.clear();
        m_meshes.clear();
        for (GpuStaticList &staticList : m_staticLists) {
            if (staticList.instances.IsValid()) {
                frame.DeferFree(m_bindlessHeap, staticList.instanceBufferIndex);
                staticList.instances.Destroy(frame);
            }
        }
        m_staticLists.clear();
    }

    uint32_t GpuResources::AddGeometry(GpuGeometry geometry) {
        m_geometries.push_back(std::move(geometry));
        return static_cast<uint32_t>(m_geometries.size() - 1);
    }

    MeshHandle GpuResources::AddMesh(const GpuMesh &mesh) {
        m_meshes.push_back(mesh);
        return MeshHandle{.index = static_cast<uint32_t>(m_meshes.size() - 1)};
    }

    void GpuResources::PrepareUpload(uint64_t bytes) {
        constexpr uint64_t kUploadBatchBytes = 64ull * 1024 * 1024;
        if (m_upload.IsRecording() && m_upload.StagingBytes() > 0 &&
            m_upload.StagingBytes() + bytes > kUploadBatchBytes) {
            m_upload.Submit(m_queue);
        }
        if (!m_upload.IsRecording()) {
            m_upload.Begin(m_queue);
        }
    }

    TextureHandle GpuResources::CreateSolidColorTexture(uint8_t red, uint8_t green, uint8_t blue,
                                                        std::wstring_view name) {
        const TextureData data{
            .dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            .isSrgb = true,
            .mips = {TextureMip{.width = 1, .height = 1, .offset = 0, .rowPitch = 4, .slicePitch = 4}},
            .pixels = {std::byte{red}, std::byte{green}, std::byte{blue}, std::byte{255}},
        };
        return CreateTexture(data, name);
    }
} // namespace render
