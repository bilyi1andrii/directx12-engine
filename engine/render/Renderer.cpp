#include "render/Renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "renderdata/Vertex.h"
#include "rhi/UploadRing.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace engine::render {
    using core::ThrowIfFailed;

    namespace {
        static_assert(sizeof(gpu::DrawConstants) <= gpu::kDrawConstantCount * sizeof(uint32_t),
                      "DrawConstants must fit in the root constants of b0 (common/root_signature.hlsli)");

        // Bytes per element of every StructuredBuffer<InstanceData>, static or per frame
        constexpr uint32_t kInstanceStride = sizeof(gpu::InstanceData);

        // How the GPU reads one Vertex (renderdata/Vertex.h): must match VSInput in vs_mesh.hlsl
        constexpr std::array kVertexLayout{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, position),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TEXCOORD",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, uv),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "NORMAL",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, normal),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TANGENT",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(Vertex, tangent),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
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
          m_bindlessHeap(m_device.Native(), rhi::BindlessHeap::kDefaultCapacity, L"Bindless heap"),
          m_upload(m_device.Native(), m_allocator, L"Upload"),
          m_context(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list"),
          m_queue(m_device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue"),
          m_rtvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap"),
          m_dsvAllocator(m_device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, L"DSV heap"),
          m_swapChain(m_device, m_queue, m_rtvAllocator, hwnd, width, height),
          m_depth(m_device.Native(), m_allocator, m_dsvAllocator, width, height, L"Depth buffer"),
          m_pipelines(m_device.Native(), core::ExecutableDirectory() / L"shaders") {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), m_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  std::format(L"Frame {}", i));
        }
        CreatePipelines();

        // All startup uploads in one batch, submitted before the first frame
        m_upload.Begin(m_queue);
        try {
            CreateFallbackTextures();
            m_upload.Submit(m_queue);
        } catch (...) {
            // ~Renderer does not run when the constructor throws, so clean up the textures and buffers here
            DestroyResources();
            throw;
        }
    }

    Renderer::~Renderer() {
        // The destructor body runs before any member is destroyed. Members declared after the
        // swap chain (depth buffer, PSO, meshes, static lists) are released before its flush, so wait
        // here (DestroyResources flushes).
        DestroyResources();
    }

    std::vector<MeshHandle> Renderer::CreateMesh(const MeshData &mesh, std::wstring_view name) {
        ENGINE_ASSERT_MSG(!mesh.vertices.empty() && !mesh.indices.empty(), "CreateMesh with {} vertices and {} indices",
                          mesh.vertices.size(), mesh.indices.size());

        // EndFrame submits this batch before the frame, so the meshes can be drawn right away
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

    void Renderer::PrepareUpload(uint64_t bytes) {
        constexpr uint64_t kUploadBatchBytes = 64ull * 1024 * 1024;
        if (m_upload.IsRecording() && m_upload.StagingBytes() > 0 &&
            m_upload.StagingBytes() + bytes > kUploadBatchBytes) {
            m_upload.Submit(m_queue);
        }
        if (!m_upload.IsRecording()) {
            m_upload.Begin(m_queue);
        }
    }

    TextureHandle Renderer::CreateTexture(const TextureData &data, std::wstring_view name) {
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

        // EndFrame submits this batch before the frame
        PrepareUpload(pixels.size_bytes());
        rhi::Texture texture =
            rhi::Texture::Create(m_allocator, m_upload, m_bindlessHeap, static_cast<DXGI_FORMAT>(data.dxgiFormat),
                                 std::span(mips).first(data.mips.size()), name);
        const TextureHandle handle{.index = texture.BindlessIndex()};
        m_textures.push_back(std::move(texture));
        return handle;
    }

    void Renderer::CreateFallbackTextures() {
        // Grey first, so it gets bindless index 0: a default handle then points to a texture
        m_unloadedTexture = CreateSolidColorTexture(100, 100, 100, L"Fallback/Unloaded (grey)");
        ENGINE_ASSERT_MSG(m_unloadedTexture.index == 0, "The grey fallback texture got bindless index {}, not 0",
                          m_unloadedTexture.index);
        m_unhandledTexture = CreateSolidColorTexture(250, 0, 0, L"Fallback/Unhandled (red)");
    }

    TextureHandle Renderer::CreateSolidColorTexture(uint8_t red, uint8_t green, uint8_t blue, std::wstring_view name) {
        // sRGB, like all colour textures
        const TextureData data{
            .dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            .isSrgb = true,
            .mips = {TextureMip{.width = 1, .height = 1, .offset = 0, .rowPitch = 4, .slicePitch = 4}},
            .pixels = {std::byte{red}, std::byte{green}, std::byte{blue}, std::byte{255}},
        };
        return CreateTexture(data, name);
    }

    void Renderer::DestroyResources() noexcept {
        try {
            m_queue.Flush();
        } catch (const std::exception &e) {
            core::LogError("Renderer: flush at destruction failed: {}", e.what());
        }
        // Destroy hands each resource to a frame slot; the slots free them when the renderer is destroyed
        for (rhi::Texture &texture : m_textures) {
            if (texture.IsValid()) {
                texture.Destroy(CurrentFrame(), m_bindlessHeap);
            }
        }
        m_textures.clear();
        for (GpuGeometry &geometry : m_geometries) {
            for (rhi::Buffer *buffer : {&geometry.vertices, &geometry.indices}) {
                if (buffer->IsValid()) {
                    buffer->Destroy(CurrentFrame());
                }
            }
        }
        m_geometries.clear();
        m_meshes.clear();
        for (GpuStaticList &staticList : m_staticLists) {
            if (staticList.instances.IsValid()) {
                CurrentFrame().DeferFree(m_bindlessHeap, staticList.instanceBufferIndex);
                staticList.instances.Destroy(CurrentFrame());
            }
        }
        m_staticLists.clear();
    }

    void Renderer::BeginFrame(uint32_t width, uint32_t height, const RendererSettings &settings) {
        m_settings = settings;

        // This slot was last used kFramesInFlight frames ago; wait until the GPU has finished that
        // frame before reusing its allocator
        rhi::FrameContext &frame = CurrentFrame();
        frame.WaitAndReset(m_queue);
        // Frees the staging memory of a finished upload batch
        m_upload.ReleaseCompleted(m_queue);

        // After WaitAndReset: objects a resize defers into this slot must survive until this
        // frame's fence, not be dropped by the reset
        if (m_swapChain.Resize(width, height)) {
            m_depth.Resize(m_swapChain.Width(), m_swapChain.Height(), frame);
        }

        m_context.Begin(frame.CommandAllocator());
        // Before any root signature is set: shaders index this heap (bindless), and only one can be bound
        m_context.SetDescriptorHeap(m_bindlessHeap.Native());
    }

    void Renderer::Render(const FrameView &view, const RenderList &list,
                          std::span<const StaticListHandle> staticLists) {
        ID3D12Resource *backBuffer = m_swapChain.CurrentBackBuffer();
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.CurrentRtv();

        // The sky: linear values, so through the sRGB view the screen shows about (56, 69, 108) of 255
        constexpr std::array clearColor{0.04f, 0.06f, 0.15f, 1.0f};
        m_context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_context.ClearRenderTarget(rtv, clearColor);
        // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
        m_context.ClearDepth(m_depth.Dsv(), rhi::DepthBuffer::kClearDepth);

        m_context.SetRenderTarget(rtv, m_depth.Dsv());
        m_context.SetViewportAndScissor(m_swapChain.Width(), m_swapChain.Height());
        m_context.SetGraphicsRootSignature(m_pipelines.RootSignature());

        // Per-frame constants go into this frame slot's upload ring: the GPU reads them while the CPU
        // already records the next frame into the other slot's ring
        const gpu::FrameConstants frameConstants = MakeFrameConstants(view);
        const rhi::UploadAllocation frameCb =
            CurrentFrame().Upload().Write(frameConstants, rhi::kConstantBufferAlignment);
        m_context.SetGraphicsRootConstantBufferView(gpu::kRootFrameConstants, frameCb.gpu);

        UploadDynamicInstances(list);
        m_context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // Static lists first, then the dynamic objects (DX11 GeometrySystem::Render does the same)
        for (const StaticListHandle handle : staticLists) {
            RecordDraws(StaticList(handle).draws);
        }
        RecordDraws(m_dynamicDraws);
    }

    template <typename BeginChunk>
    void Renderer::WriteInstances(const RenderList &list, uint32_t chunkCapacity, BeginChunk &&beginChunk,
                                  std::vector<InstanceDraw> &draws) {
        SplitIntoChunks(list.Buckets(), chunkCapacity, m_instanceRanges);
        // Reserve first: once a chunk holds a bindless slot, nothing below may throw
        draws.reserve(draws.size() + m_instanceRanges.size());

        const std::span<const BatchBucket> buckets = list.Buckets();
        const uint64_t instanceCount = list.InstanceCount();
        InstanceChunk chunk{};
        uint32_t chunksStarted = 0;
        gpu::InstanceData instance{};
        for (const InstanceRange &range : m_instanceRanges) {
            if (range.chunk == chunksStarted) {
                // First range of a new chunk: start the whole chunk (all but the last chunk are full)
                const auto elementCount = static_cast<uint32_t>(
                    std::min<uint64_t>(chunkCapacity, instanceCount - uint64_t{range.chunk} * chunkCapacity));
                chunk = beginChunk(elementCount);
                ++chunksStarted;
            }

            const BatchBucket &bucket = buckets[range.bucket];
            draws.push_back({.key = bucket.key,
                             .instanceBufferIndex = chunk.instanceBufferIndex,
                             .firstElement = range.firstElement,
                             .count = range.count});

            // Written in order, one element after the other: upload memory is write-combined (see UploadRing)
            uint64_t offset = uint64_t{range.firstElement} * kInstanceStride;
            for (uint32_t i = 0; i < range.count; ++i) {
                const DirectX::XMFLOAT4X4 &world = bucket.worlds[range.firstObject + i];
                DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&world)));
                chunk.memory.Write(offset, instance);
                offset += kInstanceStride;
            }
        }
    }

    StaticListHandle Renderer::CreateStaticList(const RenderList &list, std::wstring_view name) {
        // Reserve first: once the bindless slot is taken, nothing may throw, or the slot would leak
        m_staticLists.reserve(m_staticLists.size() + 1);
        // Filled by beginChunk. A Buffer cannot be assigned, so it is constructed in place (emplace).
        std::optional<rhi::Buffer> instances;
        uint32_t instanceBufferIndex = 0;
        std::vector<InstanceDraw> draws;
        const uint64_t instanceCount = list.InstanceCount();
        if (instanceCount > 0) {
            // EndFrame submits this batch before the frame
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
            WriteInstances(list, static_cast<uint32_t>(instanceCount), beginChunk, draws);
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

    void Renderer::UploadDynamicInstances(const RenderList &list) {
        rhi::FrameContext &frame = CurrentFrame();
        const auto beginChunk = [&](uint32_t elementCount) {
            // Aligned to the element size, so the view can start at a whole element
            InstanceChunk chunk{.memory =
                                    frame.Upload().Allocate(uint64_t{elementCount} * kInstanceStride, kInstanceStride)};
            // A fresh bindless slot every frame: the previous frame may still read the old one. It is freed after
            // this frame's fence.
            chunk.instanceBufferIndex = m_bindlessHeap.Allocate();
            frame.DeferFree(m_bindlessHeap, chunk.instanceBufferIndex);
            m_bindlessHeap.CreateStructuredBufferSrv(chunk.instanceBufferIndex, chunk.memory.resource,
                                                     chunk.memory.offset / kInstanceStride, elementCount,
                                                     kInstanceStride);
            return chunk;
        };
        // A shader view (SRV) covers one buffer, and every ring page is its own buffer: chunks of at most a page
        m_dynamicDraws.clear();
        WriteInstances(list, static_cast<uint32_t>(frame.Upload().PageSize() / kInstanceStride), beginChunk,
                       m_dynamicDraws);
    }

    void Renderer::RecordDraws(std::span<const InstanceDraw> draws) {
        // Root constants (b0) are copied into the command list, so every draw keeps its own values
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        for (const InstanceDraw &draw : draws) {
            const GpuMesh &mesh = Mesh(draw.key.mesh);
            const GpuGeometry &geometry = m_geometries[mesh.geometry];
            // Switch PSOs only when needed: it costs CPU and GPU time. The PSO depends on the geometry's vertex
            // format and on the material, so it is checked for every draw.
            const rhi::PipelineHandle pipeline = PipelineFor(geometry.vertexFormat, draw.key.material);
            if (pipeline != currentPipeline) {
                m_context.SetPipelineState(m_pipelines.Native(pipeline));
                currentPipeline = pipeline;
            }
            // The meshes of one model share their buffers, and a bucket split between two chunks gives two draws
            // of the same mesh: bind the buffers only when the geometry changes
            if (mesh.geometry != currentGeometry) {
                m_context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                m_context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            // Every draw reads indexCount indices from firstIndex on, and the GPU adds baseVertex to each
            gpu::DrawConstants constants{.instanceOffset = draw.firstElement,
                                         .materialIndex = draw.key.material.index,
                                         .instanceBufferIndex = draw.instanceBufferIndex};
            if (m_settings.drawPath == DrawPath::Instanced) {
                // One draw for all `count` objects: object i reads element instanceOffset + SV_InstanceID (= i)
                m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                m_context.DrawIndexedInstanced(mesh.indexCount, draw.count, mesh.firstIndex, mesh.baseVertex);
            } else {
                // One draw per object, each with its own element in the root constants (plan §2.6: keeps
                // the per-draw cost measurable)
                for (uint32_t i = 0; i < draw.count; ++i) {
                    constants.instanceOffset = draw.firstElement + i;
                    m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                    m_context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
                }
            }
        }
    }

    const Renderer::GpuMesh &Renderer::Mesh(MeshHandle handle) const {
        ENGINE_ASSERT_MSG(handle.index < m_meshes.size(), "Mesh handle {} out of range ({} meshes)", handle.index,
                          m_meshes.size());
        return m_meshes[handle.index];
    }

    const Renderer::GpuStaticList &Renderer::StaticList(StaticListHandle handle) const {
        ENGINE_ASSERT_MSG(handle.index < m_staticLists.size(), "Static list handle {} out of range ({} lists)",
                          handle.index, m_staticLists.size());
        return m_staticLists[handle.index];
    }

    uint32_t Renderer::AddGeometry(GpuGeometry geometry) {
        m_geometries.push_back(std::move(geometry));
        return static_cast<uint32_t>(m_geometries.size() - 1);
    }

    MeshHandle Renderer::AddMesh(const GpuMesh &mesh) {
        ENGINE_ASSERT_MSG(mesh.geometry < m_geometries.size(), "Mesh of geometry {} out of range ({} geometries)",
                          mesh.geometry, m_geometries.size());
        m_meshes.push_back(mesh);
        return MeshHandle{.index = static_cast<uint32_t>(m_meshes.size() - 1)};
    }

    void Renderer::EndFrame(bool vsync) {
        m_context.Transition(m_swapChain.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_PRESENT);
        m_context.Close();

        // Uploads first: this frame may already use them, and the GPU runs lists in submission order
        if (m_upload.IsRecording()) {
            m_upload.Submit(m_queue);
        }

        // Submit, present, and remember which fence value marks the end of this frame's GPU work
        m_queue.ExecuteCommandList(m_context.Native());
        m_swapChain.Present(vsync);
        CurrentFrame().SetFenceValue(m_queue.Signal());
        // Logs this frame's DXGI messages (Present, ResizeBuffers) next to the frame that caused them
        m_device.DrainDxgiMessages();
        ++m_frameNumber;
    }

    void Renderer::CreatePipelines() {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};

        m_pipelineByFormat[static_cast<size_t>(VertexFormat::Mesh)] = m_pipelines.Get({
            .vertexShader = L"vs_mesh.cso",
            .pixelShader = L"ps_unlit.cso",
            .inputLayout = kVertexLayout,
            .renderTargetFormats = kBackBufferTargets,
            .depthFormat = rhi::DepthBuffer::kFormat,
            .name = L"Unlit textured PSO",
        });
    }

    rhi::PipelineHandle Renderer::PipelineFor(VertexFormat format, MaterialHandle /*material*/) const {
        const rhi::PipelineHandle pipeline = m_pipelineByFormat[static_cast<size_t>(format)];
        ENGINE_ASSERT_MSG(pipeline.IsValid(), "No PSO for vertex format {}", static_cast<int>(format));
        return pipeline;
    }
} // namespace engine::render