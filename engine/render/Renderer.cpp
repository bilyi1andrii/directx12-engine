#include "render/Renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>
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

        // How the GPU reads one VertexPosColor: must match VSInput in vs_basic.hlsl
        constexpr std::array kVertexPosColorLayout{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(VertexPosColor, position),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "COLOR",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = offsetof(VertexPosColor, color),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
        };

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

        // A cube with 4 vertices per face, so every face shows the whole texture and has its own normal.
        MeshData MakeTexturedCube() {
            using DirectX::XMFLOAT3;
            // Each face as seen from outside: its normal and which way is right and up on it
            struct Face {
                XMFLOAT3 normal;
                XMFLOAT3 right;
                XMFLOAT3 up;
            };
            constexpr std::array<Face, 6> kFaces{{
                {{0, 0, -1}, {1, 0, 0}, {0, 1, 0}},  // -Z
                {{0, 0, 1}, {-1, 0, 0}, {0, 1, 0}},  // +Z
                {{-1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, // -X
                {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}},   // +X
                {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},   // +Y, seen from above with +Z up
                {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}}, // -Y, seen from below with -Z up
            }};
            // A corner of the face: r and u are -1 or +1
            const auto corner = [](const Face &face, float r, float u) {
                return XMFLOAT3{face.normal.x + r * face.right.x + u * face.up.x,
                                face.normal.y + r * face.right.y + u * face.up.y,
                                face.normal.z + r * face.right.z + u * face.up.z};
            };

            MeshData cube;
            cube.vertices.reserve(kFaces.size() * 4);
            cube.indices.reserve(kFaces.size() * 6);
            for (const Face &face : kFaces) {
                const auto first = static_cast<uint32_t>(cube.vertices.size());
                // Bottom-left, top-left, top-right, bottom-right. UV (0, 0) is the top-left of the texture.
                cube.vertices.push_back({corner(face, -1, -1), {0, 1}, face.normal, face.right});
                cube.vertices.push_back({corner(face, -1, 1), {0, 0}, face.normal, face.right});
                cube.vertices.push_back({corner(face, 1, 1), {1, 0}, face.normal, face.right});
                cube.vertices.push_back({corner(face, 1, -1), {1, 1}, face.normal, face.right});
                // Two clockwise triangles
                cube.indices.insert(cube.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
            }
            cube.submeshes.push_back({.firstIndex = 0, .indexCount = static_cast<uint32_t>(cube.indices.size())});
            return cube;
        }

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
            UploadCubes();
            CreateFallbackTextures();
            m_upload.Submit(m_queue);
        } catch (...) {
            // ~Renderer does not run when the constructor throws, so clean up the textures and buffers here
            DestroyGpuResources();
            throw;
        }
    }

    Renderer::~Renderer() {
        // The destructor body runs before any member is destroyed. Members declared after the
        // swap chain (depth buffer, PSO, meshes) are released before its flush, so wait here
        // (DestroyGpuResources flushes).
        DestroyGpuResources();
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

    void Renderer::DestroyGpuResources() noexcept {
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

    void Renderer::Render(const FrameView &view, const RenderList &list) {
        ID3D12Resource *backBuffer = m_swapChain.CurrentBackBuffer();
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.CurrentRtv();

        constexpr std::array clearColor{0.4f, 0.6f, 0.9f, 1.0f};
        m_context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_context.ClearRenderTarget(rtv, clearColor);
        // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
        m_context.ClearDepth(m_depth.Dsv(), rhi::DepthBuffer::kClearDepth);

        m_context.SetRenderTarget(rtv, m_depth.Dsv());
        m_context.SetViewportAndScissor(m_swapChain.Width(), m_swapChain.Height());
        m_context.SetGraphicsRootSignature(m_pipelines.RootSignature());

        // Per-frame constants go into this frame slot's upload ring: the GPU reads them while the CPU
        // already records the next frame into the other slot's ring
        gpu::FrameConstants frameConstants = MakeFrameConstants(view);
        frameConstants.instanceBufferIndex = UploadInstances(list);
        const rhi::UploadAllocation frameCb =
            CurrentFrame().Upload().Write(frameConstants, rhi::kConstantBufferAlignment);
        m_context.SetGraphicsRootConstantBufferView(gpu::kRootFrameConstants, frameCb.gpu);

        m_context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        DrawBuckets(list);
    }

    uint32_t Renderer::UploadInstances(const RenderList &list) {
        const size_t instanceCount = list.InstanceCount();
        if (instanceCount == 0) {
            return 0; // no draws, so no shader reads the index
        }

        // All buckets go into one allocation, in bucket order (plan §3.2). Aligned to the element size, so
        // the allocation starts at a whole element of the page seen as an InstanceData array.
        constexpr uint64_t kStride = sizeof(gpu::InstanceData);
        rhi::FrameContext &frame = CurrentFrame();
        const rhi::UploadAllocation instances = frame.Upload().Allocate(instanceCount * kStride, kStride);

        // Written in order, one element after the other: upload memory is write-combined (see UploadRing)
        gpu::InstanceData instance{};
        uint64_t offset = 0;
        for (const BatchBucket &bucket : list.Buckets()) {
            for (const DirectX::XMFLOAT4X4 &world : bucket.worlds) {
                DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&world)));
                instances.Write(offset, instance);
                offset += kStride;
            }
        }

        // A shader resource view (SRV) over exactly these elements, in a fresh bindless slot: the data
        // moves around the ring from frame to frame, and a slot must not be rewritten while the previous
        // frame may still read it. The slot goes back to the heap after this frame's fence.
        const uint32_t index = m_bindlessHeap.Allocate();
        frame.DeferFree(m_bindlessHeap, index);
        m_bindlessHeap.CreateStructuredBufferSrv(index, instances.resource, instances.offset / kStride,
                                                 static_cast<uint32_t>(instanceCount), kStride);
        return index;
    }

    void Renderer::DrawBuckets(const RenderList &list) {
        // Root constants (b0) are copied into the command list, so every draw keeps its own values

        // Element of the current bucket's first object in the instance buffer (UploadInstances' order)
        uint32_t firstInstance = 0;
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        for (const BatchBucket &bucket : list.Buckets()) {
            const GpuMesh &mesh = Mesh(bucket.key.mesh);
            const GpuGeometry &geometry = m_geometries[mesh.geometry];
            // Switch PSOs only when needed: it costs CPU and GPU time
            const rhi::PipelineHandle pipeline = PipelineFor(geometry.vertexFormat, bucket.key.material);
            if (pipeline != currentPipeline) {
                m_context.SetPipelineState(m_pipelines.Native(pipeline));
                currentPipeline = pipeline;
            }
            // The meshes of one model share their buffers, so consecutive buckets often need no rebinding
            if (mesh.geometry != currentGeometry) {
                m_context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                m_context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            // Every draw reads indexCount indices from firstIndex on, and the GPU adds baseVertex to each
            const auto objectCount = static_cast<uint32_t>(bucket.worlds.size());
            gpu::DrawConstants constants{.instanceOffset = firstInstance, .materialIndex = bucket.key.material.index};
            if (m_settings.drawPath == DrawPath::Instanced) {
                // One draw for the whole bucket: object i reads element instanceOffset + SV_InstanceID (= i)
                m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                m_context.DrawIndexedInstanced(mesh.indexCount, objectCount, mesh.firstIndex, mesh.baseVertex);
            } else {
                // One draw per object, each with its own element in the root constants (plan §2.6: keeps
                // the per-draw cost measurable)
                for (uint32_t i = 0; i < objectCount; ++i) {
                    constants.instanceOffset = firstInstance + i;
                    m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                    m_context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
                }
            }
            firstInstance += objectCount;
        }
    }

    const Renderer::GpuMesh &Renderer::Mesh(MeshHandle handle) const {
        ENGINE_ASSERT_MSG(handle.index < m_meshes.size(), "Mesh handle {} out of range ({} meshes)", handle.index,
                          m_meshes.size());
        return m_meshes[handle.index];
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

    void Renderer::UploadCubes() {
        // A geometry with a single mesh that covers all of it, in its own vertex format
        const uint32_t colorCube = AddGeometry({
            .vertices = rhi::Buffer::CreateWithData(m_allocator, m_upload, kCubeVertices,
                                                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"Cube/VB"),
            .indices = rhi::Buffer::CreateWithData(m_allocator, m_upload, kCubeIndices,
                                                   D3D12_RESOURCE_STATE_INDEX_BUFFER, L"Cube/IB"),
            .vertexStride = sizeof(VertexPosColor),
            .indexFormat = DXGI_FORMAT_R16_UINT,
            .vertexFormat = VertexFormat::PositionColor,
        });
        m_cubeMesh = AddMesh({.geometry = colorCube, .indexCount = static_cast<uint32_t>(kCubeIndices.size())});

        // Built like a model, so every run also goes through CreateMesh
        m_texturedCubeMesh = CreateMesh(MakeTexturedCube(), L"TexturedCube").front();
    }

    void Renderer::CreatePipelines() {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};

        m_pipelineByFormat[static_cast<size_t>(VertexFormat::PositionColor)] = m_pipelines.Get({
            .vertexShader = L"vs_basic.cso",
            .pixelShader = L"ps_basic.cso",
            .inputLayout = kVertexPosColorLayout,
            .renderTargetFormats = kBackBufferTargets,
            .depthFormat = rhi::DepthBuffer::kFormat,
            .name = L"Vertex colour PSO",
        });
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