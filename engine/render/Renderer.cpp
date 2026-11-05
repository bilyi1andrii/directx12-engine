#include "render/Renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
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
#include "rhi/UploadRing.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace engine::render {
    using core::ThrowIfFailed;

    namespace {
        static_assert(sizeof(gpu::DrawConstants) <= gpu::kDrawConstantCount * sizeof(uint32_t),
                      "DrawConstants must fit in the root constants of b0 (common/root_signature.hlsli)");

        // Compiled shader bytecode (DXIL), written by the build to <exe dir>/shaders/<name>.cso
        std::vector<std::byte> LoadShader(std::wstring_view fileName) {
            auto bytecode = core::ReadBinaryFile(core::ExecutableDirectory() / L"shaders" / fileName);
            if (!bytecode) {
                throw std::runtime_error(std::format("Cannot load shader {}", bytecode.error().message));
            }
            return std::move(*bytecode);
        }

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

        // The mesh vertex, same layout as DX11 Graphics/Vertex.h. Must match VSInput in vs_mesh.hlsl.
        // Lives here until model loading needs it elsewhere.
        struct Vertex {
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT2 uv;
            DirectX::XMFLOAT3 normal;
            DirectX::XMFLOAT3 tangent; // for normal mapping later
        };
        static_assert(sizeof(Vertex) == 44, "Vertex must keep the DX11 layout: 44 bytes, no padding");

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

        // A cube with 4 vertices per face, so every face shows the whole texture and has its own normal
        struct TexturedCube {
            std::array<Vertex, 24> vertices;
            std::array<uint16_t, 36> indices;
        };

        TexturedCube MakeTexturedCube() {
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

            TexturedCube cube{};
            for (uint16_t f = 0; f < kFaces.size(); ++f) {
                const Face &face = kFaces[f];
                const auto first = static_cast<uint16_t>(f * 4);
                // Bottom-left, top-left, top-right, bottom-right. UV (0, 0) is the top-left of the texture.
                cube.vertices[first + 0] = {corner(face, -1, -1), {0, 1}, face.normal, face.right};
                cube.vertices[first + 1] = {corner(face, -1, 1), {0, 0}, face.normal, face.right};
                cube.vertices[first + 2] = {corner(face, 1, 1), {1, 0}, face.normal, face.right};
                cube.vertices[first + 3] = {corner(face, 1, -1), {1, 1}, face.normal, face.right};
                // Two clockwise triangles
                const std::array<uint16_t, 6> faceIndices{
                    first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2),
                    first, static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)};
                std::ranges::copy(faceIndices, cube.indices.begin() + f * 6);
            }
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
          m_depth(m_device.Native(), m_allocator, m_dsvAllocator, width, height, L"Depth buffer") {
        m_frames.reserve(rhi::kFramesInFlight);
        for (uint32_t i = 0; i < rhi::kFramesInFlight; ++i) {
            m_frames.emplace_back(m_device.Native(), m_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  std::format(L"Frame {}", i));
        }
        // The global root signature is defined in HLSL (shaders/common/root_signature.hlsli) and compiled by
        // DXC into its own file, already validated and serialized: the device only deserializes it
        const std::vector<std::byte> rootSignature = LoadShader(L"rs_global.cso");
        ThrowIfFailed(m_device.Native()->CreateRootSignature(0, rootSignature.data(), rootSignature.size(),
                                                             IID_PPV_ARGS(&m_rootSignature)));
        ThrowIfFailed(m_rootSignature->SetName(L"Global root signature"));
        m_vertexColorPipeline =
            CreatePipelineState(L"vs_basic.cso", L"ps_basic.cso", kVertexPosColorLayout, L"Vertex colour PSO");
        m_unlitPipeline = CreatePipelineState(L"vs_mesh.cso", L"ps_unlit.cso", kVertexLayout, L"Unlit textured PSO");

        // All startup uploads in one batch, submitted before the first frame
        m_upload.Begin(m_queue);
        try {
            UploadCubes();
            CreateFallbackTextures();
            m_upload.Submit(m_queue);
        } catch (...) {
            // ~Renderer does not run when the constructor throws, so clean up the textures here
            DestroyTextures();
            throw;
        }
    }

    Renderer::~Renderer() {
        // The destructor body runs before any member is destroyed. Members declared after the
        // swap chain (depth buffer, PSO, meshes) are released before its flush, so wait here
        // (DestroyTextures flushes).
        DestroyTextures();
    }

    TextureHandle Renderer::CreateTexture(const TextureData &data, std::wstring_view name) {
        std::array<rhi::TextureMipData, D3D12_REQ_MIP_LEVELS> mips{};
        ENGINE_ASSERT_MSG(!data.mips.empty() && data.mips.size() <= mips.size(), "Texture with {} mip levels",
                          data.mips.size());
        const std::span<const std::byte> pixels(data.pixels);
        for (size_t i = 0; i < data.mips.size(); ++i) {
            const TextureMip &mip = data.mips[i];
            // Written so it cannot overflow; a level outside the pixels would make subspan undefined behaviour
            ENGINE_ASSERT_MSG(mip.offset <= pixels.size() && mip.slicePitch <= pixels.size() - mip.offset,
                              "Mip {} ({} bytes at offset {}) lies outside the texture's {} bytes", i, mip.slicePitch,
                              mip.offset, pixels.size());
            mips[i] = {.width = mip.width,
                       .height = mip.height,
                       .rowPitch = mip.rowPitch,
                       .pixels = pixels.subspan(mip.offset, mip.slicePitch)};
        }

        // EndFrame submits this batch before the frame
        if (!m_upload.IsRecording()) {
            m_upload.Begin(m_queue);
        }
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

    void Renderer::DestroyTextures() noexcept {
        try {
            m_queue.Flush();
        } catch (const std::exception &e) {
            core::LogError("Renderer: flush at destruction failed: {}", e.what());
        }
        // Destroy hands each texture to a frame slot; the slots free them when the renderer is destroyed
        for (rhi::Texture &texture : m_textures) {
            if (texture.IsValid()) {
                texture.Destroy(CurrentFrame(), m_bindlessHeap);
            }
        }
        m_textures.clear();
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
        m_context.SetGraphicsRootSignature(m_rootSignature.Get());

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
        ID3D12PipelineState *currentPipeline = nullptr;
        for (const BatchBucket &bucket : list.Buckets()) {
            const GpuMesh &mesh = Mesh(bucket.key.mesh);
            // Switch PSOs only when needed: it costs CPU and GPU time
            if (mesh.pipelineState != currentPipeline) {
                m_context.SetPipelineState(mesh.pipelineState);
                currentPipeline = mesh.pipelineState;
            }
            m_context.SetVertexBuffer(mesh.vertices.VertexBufferView(mesh.vertexStride));
            m_context.SetIndexBuffer(mesh.indices.IndexBufferView(mesh.indexFormat));

            const auto objectCount = static_cast<uint32_t>(bucket.worlds.size());
            gpu::DrawConstants constants{.instanceOffset = firstInstance, .materialIndex = bucket.key.material.index};
            if (m_settings.drawPath == DrawPath::Instanced) {
                // One draw for the whole bucket: object i reads element instanceOffset + SV_InstanceID (= i)
                m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                m_context.DrawIndexedInstanced(mesh.indexCount, objectCount);
            } else {
                // One draw per object, each with its own element in the root constants (plan §2.6: keeps
                // the per-draw cost measurable)
                for (uint32_t i = 0; i < objectCount; ++i) {
                    constants.instanceOffset = firstInstance + i;
                    m_context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                    m_context.DrawIndexed(mesh.indexCount);
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
        GpuMesh colorCube{
            .vertices = rhi::Buffer::CreateWithData(m_allocator, m_upload, kCubeVertices,
                                                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"Cube/VB"),
            .indices = rhi::Buffer::CreateWithData(m_allocator, m_upload, kCubeIndices,
                                                   D3D12_RESOURCE_STATE_INDEX_BUFFER, L"Cube/IB"),
            .vertexStride = sizeof(VertexPosColor),
            .indexFormat = DXGI_FORMAT_R16_UINT,
            .indexCount = static_cast<uint32_t>(kCubeIndices.size()),
            .pipelineState = m_vertexColorPipeline.Get(),
        };
        m_cubeMesh = MeshHandle{.index = static_cast<uint32_t>(m_meshes.size())};
        m_meshes.push_back(std::move(colorCube));

        const TexturedCube cube = MakeTexturedCube();
        GpuMesh texturedCube{
            .vertices =
                rhi::Buffer::CreateWithData(m_allocator, m_upload, cube.vertices,
                                            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"TexturedCube/VB"),
            .indices = rhi::Buffer::CreateWithData(m_allocator, m_upload, cube.indices,
                                                   D3D12_RESOURCE_STATE_INDEX_BUFFER, L"TexturedCube/IB"),
            .vertexStride = sizeof(Vertex),
            .indexFormat = DXGI_FORMAT_R16_UINT,
            .indexCount = static_cast<uint32_t>(cube.indices.size()),
            .pipelineState = m_unlitPipeline.Get(),
        };
        m_texturedCubeMesh = MeshHandle{.index = static_cast<uint32_t>(m_meshes.size())};
        m_meshes.push_back(std::move(texturedCube));
    }

    rhi::ComPtr<ID3D12PipelineState>
    Renderer::CreatePipelineState(std::wstring_view vertexShaderFile, std::wstring_view pixelShaderFile,
                                  std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout, std::wstring_view name) {
        const std::vector<std::byte> vertexShader = LoadShader(vertexShaderFile);
        const std::vector<std::byte> pixelShader = LoadShader(pixelShaderFile);

        // A pipeline state object (PSO) bakes shaders and all fixed-function state into one object
        // up front, so the driver compiles everything once instead of at draw time. D3D12 has no
        // separate RSSetState/OMSetBlendState: changing any of this means switching PSOs.
        // The CD3DX12 defaults: no blending; solid fill, back faces culled, clockwise = front;
        // depth test LESS with depth writes on.
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
            .pRootSignature = m_rootSignature.Get(),
            .VS = {.pShaderBytecode = vertexShader.data(), .BytecodeLength = vertexShader.size()},
            .PS = {.pShaderBytecode = pixelShader.data(), .BytecodeLength = pixelShader.size()},
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = D3D12_DEFAULT_SAMPLE_MASK, // all samples
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .InputLayout = {.pInputElementDescs = inputLayout.data(),
                            .NumElements = static_cast<UINT>(inputLayout.size())},
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            // The RTV's format (sRGB), not the back buffer's
            .RTVFormats = {rhi::SwapChain::kRtvFormat},
            // Must match the depth buffer; checked against the bound DSV when drawing
            .DSVFormat = rhi::DepthBuffer::kFormat,
            .SampleDesc = {.Count = 1, .Quality = 0},
        };

        rhi::ComPtr<ID3D12PipelineState> pipelineState;
        ThrowIfFailed(m_device.Native()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState)));
        ThrowIfFailed(pipelineState->SetName(std::wstring(name).c_str()));
        return pipelineState;
    }
} // namespace engine::render