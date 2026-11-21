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
#include "renderdata/Vertex.h"
#include "rhi/Allocator.h"
#include "rhi/BindlessHeap.h"
#include "rhi/CommandQueue.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadContext.h"

namespace engine::render {
    GpuResources::GpuResources(rhi::Allocator &allocator, rhi::UploadContext &upload, rhi::CommandQueue &queue,
                               rhi::BindlessHeap &bindlessHeap) noexcept
        : m_allocator(allocator), m_upload(upload), m_queue(queue), m_bindlessHeap(bindlessHeap) {}

    GpuResources::~GpuResources() {
        // Buffers and textures released here, without DestroyAll, could still be in use by the GPU
        ENGINE_ASSERT_MSG(m_textures.empty() && m_geometries.empty() && m_staticLists.empty(),
                          "GpuResources destroyed without DestroyAll ({} textures, {} geometries, {} static lists)",
                          m_textures.size(), m_geometries.size(), m_staticLists.size());
    }

    void GpuResources::CreateDefaults() {
        ENGINE_ASSERT_MSG(m_textures.empty() && m_materials.empty(), "CreateDefaults called after other resources");
        // Before any other texture, so it gets bindless index 0: a default handle then points to a texture
        m_unloadedTexture = CreateSolidColorTexture(100, 100, 100, L"Fallback/Unloaded (grey)");
        ENGINE_ASSERT_MSG(m_unloadedTexture.index == 0, "The grey fallback texture got bindless index {}, not 0",
                          m_unloadedTexture.index);
        // Material 0, so a default MaterialHandle draws the grey texture
        m_materials.push_back(MaterialInfo{.albedo = m_unloadedTexture});
    }

    std::vector<MeshHandle> GpuResources::CreateMesh(const MeshData &mesh, std::wstring_view name) {
        ENGINE_ASSERT_MSG(!mesh.vertices.empty() && !mesh.indices.empty(), "CreateMesh with {} vertices and {} indices",
                          mesh.vertices.size(), mesh.indices.size());

        // The owner submits this batch before the frame, so the meshes can be drawn right away
        PrepareUpload(std::span(mesh.vertices).size_bytes() + std::span(mesh.indices).size_bytes());
        const std::wstring prefix(name);
        const uint32_t geometry = AddGeometry({
            .vertices = rhi::Buffer::CreateWithData(m_allocator, m_upload, mesh.vertices,
                                                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, prefix + L"/VB"),
            .indices = rhi::Buffer::CreateWithData(m_allocator, m_upload, mesh.indices,
                                                   D3D12_RESOURCE_STATE_INDEX_BUFFER, prefix + L"/IB"),
            .vertexStride = sizeof(Vertex),
            .indexFormat = DXGI_FORMAT_R32_UINT, // MeshData's indices are 32-bit: models can exceed 65536 vertices
            .vertexFormat = VertexFormat::Mesh,
        });

        std::vector<MeshHandle> handles;
        handles.reserve(mesh.submeshes.size());
        for (const SubmeshData &submesh : mesh.submeshes) {
            // A range outside the arrays would draw parts of other submeshes, or read past the buffers
            ENGINE_ASSERT_MSG(submesh.firstIndex <= mesh.indices.size() &&
                                  submesh.indexCount <= mesh.indices.size() - submesh.firstIndex,
                              "Submesh indices {}+{} outside the {} indices", submesh.firstIndex, submesh.indexCount,
                              mesh.indices.size());
            ENGINE_ASSERT_MSG(submesh.baseVertex >= 0 && static_cast<size_t>(submesh.baseVertex) < mesh.vertices.size(),
                              "Submesh base vertex {} outside the {} vertices", submesh.baseVertex,
                              mesh.vertices.size());
            handles.push_back(AddMesh({
                .geometry = geometry,
                .firstIndex = submesh.firstIndex,
                .indexCount = submesh.indexCount,
                .baseVertex = submesh.baseVertex,
            }));
        }
        return handles;
    }

    TextureHandle GpuResources::CreateTexture(const TextureData &data, std::wstring_view name) {
        std::array<rhi::TextureMipData, D3D12_REQ_MIP_LEVELS> mips{};
        ENGINE_ASSERT_MSG(!data.mips.empty() && data.mips.size() <= mips.size(), "Texture with {} mip levels",
                          data.mips.size());
        const std::span<const std::byte> pixels(data.pixels);
        for (size_t i = 0; i < data.mips.size(); ++i) {
            const TextureMip &mip = data.mips[i];
            // The mip must lie inside the pixels (checked in a way that cannot overflow)
            ENGINE_ASSERT_MSG(mip.offset <= pixels.size() && mip.slicePitch <= pixels.size() - mip.offset,
                              "Mip {} ({} bytes at offset {}) lies outside the texture's {} bytes", i, mip.slicePitch,
                              mip.offset, pixels.size());
            mips[i] = {.width = mip.width,
                       .height = mip.height,
                       .rowPitch = mip.rowPitch,
                       .pixels = pixels.subspan(mip.offset, mip.slicePitch)};
        }

        // The owner submits this batch before the frame
        PrepareUpload(pixels.size_bytes());
        rhi::Texture texture =
            rhi::Texture::Create(m_allocator, m_upload, m_bindlessHeap, static_cast<DXGI_FORMAT>(data.dxgiFormat),
                                 std::span(mips).first(data.mips.size()), name);
        const TextureHandle handle{.index = texture.BindlessIndex()};
        m_textures.push_back(std::move(texture));
        return handle;
    }

    MaterialHandle GpuResources::CreateMaterial(const MaterialInfo &material) {
        ENGINE_ASSERT_MSG(std::to_underlying(material.shading) < kShadingCount, "Material with unknown shading {}",
                          std::to_underlying(material.shading));
        // The unlit shader has no alpha-test or blended variant
        ENGINE_ASSERT_MSG(material.shading == Shading::Lit || material.alphaMode == AlphaMode::Opaque,
                          "Unlit material with alpha mode {}", std::to_underlying(material.alphaMode));
        m_materials.push_back(material);
        return MaterialHandle{.index = static_cast<uint32_t>(m_materials.size() - 1)};
    }

    StaticListHandle GpuResources::CreateStaticList(const RenderList &list, std::wstring_view name) {
        // Reserve first: once the bindless slot is taken, nothing may throw, or the slot would leak
        m_staticLists.reserve(m_staticLists.size() + 1);
        // Filled by beginChunk. A Buffer cannot be assigned, so it is constructed in place (emplace).
        std::optional<rhi::Buffer> instances;
        uint32_t instanceBufferIndex = 0;
        std::vector<InstanceDraw> draws;
        const uint64_t instanceCount = list.InstanceCount();
        if (instanceCount > 0) {
            // The owner submits this batch before the frame
            PrepareUpload(instanceCount * kInstanceStride);
            // A GPU buffer has no page limit: one chunk for all objects
            const auto beginChunk = [&](uint32_t elementCount) {
                // GPU-local memory, filled once by a copy like a vertex buffer, so the GPU does not read these
                // matrices across PCIe every frame. WriteInstances fills the staging memory.
                InstanceChunk chunk{};
                instances.emplace(
                    rhi::Buffer::CreateStaged(m_allocator, m_upload, uint64_t{elementCount} * kInstanceStride,
                                              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, name, chunk.memory));
                // A permanent view: unlike the ring's chunks, this buffer never moves
                instanceBufferIndex = m_bindlessHeap.Allocate();
                m_bindlessHeap.CreateStructuredBufferSrv(instanceBufferIndex, instances->Native(), 0, elementCount,
                                                         kInstanceStride);
                chunk.instanceBufferIndex = instanceBufferIndex;
                return chunk;
            };
            // Static lists are created rarely, so the ranges need no reused memory
            std::vector<InstanceRange> ranges;
            WriteInstances(list, static_cast<uint32_t>(instanceCount), beginChunk, ranges, draws);
        }

        const StaticListHandle handle{.index = static_cast<uint32_t>(m_staticLists.size())};
        // No reallocation (reserved above), and moving a Buffer or a vector cannot throw
        m_staticLists.push_back(GpuStaticList{
            .instances = instances ? std::move(*instances) : rhi::Buffer{},
            .instanceBufferIndex = instanceBufferIndex,
            .draws = std::move(draws),
        });
        return handle;
    }

    void GpuResources::DestroyAll(rhi::FrameContext &frame) noexcept {
        // Destroy hands each resource to the frame slot, which frees it once the GPU is done with that slot
        for (rhi::Texture &texture : m_textures) {
            if (texture.IsValid()) {
                texture.Destroy(frame, m_bindlessHeap);
            }
        }
        m_textures.clear();
        // The materials and the fallback handle name bindless slots that are freed with the textures
        m_unloadedTexture = {};
        m_materials.clear();
        for (GpuGeometry &geometry : m_geometries) {
            for (rhi::Buffer *buffer : {&geometry.vertices, &geometry.indices}) {
                if (buffer->IsValid()) {
                    buffer->Destroy(frame);
                }
            }
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
        ENGINE_ASSERT_MSG(mesh.geometry < m_geometries.size(), "Mesh of geometry {} out of range ({} geometries)",
                          mesh.geometry, m_geometries.size());
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
        // sRGB, like all colour textures
        const TextureData data{
            .dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            .isSrgb = true,
            .mips = {TextureMip{.width = 1, .height = 1, .offset = 0, .rowPitch = 4, .slicePitch = 4}},
            .pixels = {std::byte{red}, std::byte{green}, std::byte{blue}, std::byte{255}},
        };
        return CreateTexture(data, name);
    }
} // namespace engine::render
