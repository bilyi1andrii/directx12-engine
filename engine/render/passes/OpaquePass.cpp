#include "render/passes/OpaquePass.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

#include "core/Assert.h"
#include "render/FrameInputs.h"
#include "renderdata/Vertex.h"
#include "rhi/CommandContext.h"
#include "rhi/DepthFormat.h"
#include "rhi/SwapChain.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace engine::render {
    namespace {
        static_assert(sizeof(gpu::DrawConstants) <= gpu::kDrawConstantCount * sizeof(uint32_t),
                      "DrawConstants must fit in the root constants of b0 (common/root_signature.hlsli)");
        // Position of DrawConstants::instanceOffset in b0, in 32-bit values
        constexpr uint32_t kInstanceOffsetConstant = offsetof(gpu::DrawConstants, instanceOffset) / sizeof(uint32_t);

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
    } // namespace

    OpaquePass::OpaquePass(rhi::PipelineCache &pipelines) : m_pipelines(pipelines), m_pipelinesByFormat{} {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};

        // A PSO fixes the pixel shader and the cull mode; a command list cannot change them between draws.
        // So each combination a material can ask for gets its own PSO.
        struct Variant {
            Shading shading;
            bool doubleSided;
            std::wstring_view pixelShader;
            std::wstring_view name;
        };
        constexpr std::array kVariants{
            Variant{Shading::Lit, false, L"ps_light.cso", L"Lit PSO"},
            Variant{Shading::Lit, true, L"ps_light.cso", L"Lit two-sided PSO"},
            Variant{Shading::Unlit, false, L"ps_unlit.cso", L"Unlit PSO"},
            Variant{Shading::Unlit, true, L"ps_unlit.cso", L"Unlit two-sided PSO"},
        };
        ShadingPipelines &meshPipelines = m_pipelinesByFormat[std::to_underlying(VertexFormat::Mesh)];
        for (const Variant &variant : kVariants) {
            meshPipelines[std::to_underlying(variant.shading)][variant.doubleSided ? 1 : 0] = pipelines.Get({
                .vertexShader = L"vs_mesh.cso",
                .pixelShader = variant.pixelShader,
                .inputLayout = kVertexLayout,
                .cull = variant.doubleSided ? rhi::CullMode::None : rhi::CullMode::Back,
                .renderTargetFormats = kBackBufferTargets,
                .depthFormat = rhi::kDepthFormat,
                .name = variant.name,
            });
        }
    }

    void OpaquePass::AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth,
                           const GpuResources &resources) const {
        graph.AddPass(
            "Opaque",
            [&](PassBuilder &builder) {
                builder.WriteColor(color, ClearColor{0.04f, 0.06f, 0.15f, 1.0f}, colorView);
                // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
                builder.WriteDepth(depth, rhi::kClearDepth);
            },
            [this, &resources](PassContext &context) {
                const FrameInputs &inputs = context.Inputs();
                Record(context.Commands(), resources, inputs.drawPath, inputs.drawSets);
            });
    }

    void OpaquePass::Record(rhi::CommandContext &context, const GpuResources &resources, DrawPath drawPath,
                            std::span<const std::span<const InstanceDraw>> drawSets) const {
        context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // Root constants (b0) are copied into the command list, so every draw keeps its own values.
        // The PSO and buffers carry over from one set to the next: geometry indices are the same in every set.
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        for (const InstanceDraw &draw : drawSets | std::views::join) {
            const GpuMesh &mesh = resources.Mesh(draw.key.mesh);
            const GpuGeometry &geometry = resources.Geometry(mesh.geometry);
            const MaterialInfo &material = resources.Material(draw.key.material);
            // Switch PSOs only when needed: it costs CPU and GPU time. The PSO depends on the geometry's vertex
            // format and on the material, so it is checked for every draw.
            const rhi::PipelineHandle pipeline = PipelineFor(geometry.vertexFormat, material);
            if (pipeline != currentPipeline) {
                context.SetPipelineState(m_pipelines.Native(pipeline));
                currentPipeline = pipeline;
            }
            // The meshes of one model share their buffers, and a bucket split between two chunks gives two draws
            // of the same mesh: bind the buffers only when the geometry changes
            if (mesh.geometry != currentGeometry) {
                context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            // Every draw reads indexCount indices from firstIndex on, and the GPU adds baseVertex to each
            gpu::DrawConstants constants{.instanceOffset = draw.firstElement,
                                         .materialIndex = material.albedo.index,
                                         .instanceBufferIndex = draw.instanceBufferIndex};
            if (drawPath == DrawPath::Instanced) {
                // One draw for all `count` objects: object i reads element instanceOffset + SV_InstanceID (= i)
                context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                context.DrawIndexedInstanced(mesh.indexCount, draw.count, mesh.firstIndex, mesh.baseVertex);
            } else {
                // One draw per object, each with its own element in the root constants: this keeps the
                // per-draw cost measurable. The material and buffer are the same for all of them, so only
                // instanceOffset is sent per draw.
                context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                for (uint32_t i = 0; i < draw.count; ++i) {
                    context.SetGraphicsRootConstant(gpu::kRootDrawConstants, draw.firstElement + i,
                                                    kInstanceOffsetConstant);
                    context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
                }
            }
        }
    }

    rhi::PipelineHandle OpaquePass::PipelineFor(VertexFormat format, const MaterialInfo &material) const {
        const rhi::PipelineHandle pipeline =
            m_pipelinesByFormat[std::to_underlying(format)][std::to_underlying(material.shading)]
                               [material.doubleSided ? 1 : 0];
        ENGINE_ASSERT_MSG(pipeline.IsValid(), "No PSO for vertex format {}, shading {}", std::to_underlying(format),
                          std::to_underlying(material.shading));
        return pipeline;
    }
} // namespace engine::render
