#include "render/passes/ShadowPass.h"

#include <DirectXMath.h>

#include "render/FrameInputs.h"
#include "render/passes/MeshDrawing.h"
#include "render/passes/VertexLayout.h"
#include "renderdata/ShadowView.h"
#include "rhi/DepthFormat.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadRing.h"

namespace render {
    namespace {
        // negative, reversed-Z pushes casters away from light
        constexpr rhi::DepthBias kCasterBias{.constant = -100, .slopeScaled = -2.0f};
        constexpr float kNormalOffsetTexels = 1.5f; // receivers move along normal, fights acne
    } // namespace

    ShadowPass::ShadowPass(rhi::PipelineCache &pipelines) : m_pipelines(pipelines) {
        const auto create = [&](rhi::CullMode cull, std::wstring_view name) {
            return pipelines.Get({
                .vertexShader = L"vs_mesh_shadow.cso", // no pixel shader, depth only
                .inputLayout = kVertexLayout,
                .cull = cull,
                .depthBias = kCasterBias,
                .depthFormat = rhi::kDepthFormat,
                .name = name,
            });
        };
        m_culled = create(rhi::CullMode::Back, L"Shadow PSO");
        m_twoSided = create(rhi::CullMode::None, L"Shadow two-sided PSO");
    }

    void ShadowPass::AddTo(FrameGraph &graph, FgTexture shadowMap, const GpuResources &resources) const {
        graph.AddPass(
            "Shadow", [&](PassBuilder &builder) { builder.WriteDepth(shadowMap, rhi::kClearDepth); },
            [this, &resources](PassContext &context) {
                BindShadowConstants(context, 0); // shadow VS reads only matrix, slot unused
                const FrameInputs &inputs = context.Inputs();
                RecordMeshDraws(context.Commands(), m_pipelines, resources, inputs.drawPath, inputs.drawSets,
                                [this](VertexFormat, const MaterialInfo &material) {
                                    // light spheres and see-through objects cast no shadow
                                    if (material.shading == Shading::Unlit || material.alphaMode == AlphaMode::Blend) {
                                        return rhi::PipelineHandle{};
                                    }
                                    return material.doubleSided ? m_twoSided : m_culled;
                                });
            });
    }

    void BindShadowConstants(PassContext &context, uint32_t shadowMapIndex) {
        using namespace DirectX;
        const ShadowView &view = context.Inputs().shadow;
        gpu::ShadowConstants constants{.shadowMapIndex = shadowMapIndex,
                                       .texelSize = 1.0f / static_cast<float>(kShadowMapSize),
                                       .normalOffset = kNormalOffsetTexels * view.texelWorldSize};
        XMStoreFloat4x4(&constants.lightViewProjection, XMMatrixTranspose(XMLoadFloat4x4(&view.viewProjection)));
        const rhi::UploadAllocation buffer = context.Frame().Ring().Write(constants, rhi::kConstantBufferAlignment);
        context.Commands().SetGraphicsRootConstantBufferView(gpu::kRootPassConstants, buffer.gpu);
    }
} // namespace render
