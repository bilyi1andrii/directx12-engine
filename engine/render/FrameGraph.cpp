#include "render/FrameGraph.h"

#include <utility>

#include "core/Assert.h"
#include "core/Log.h"
#include "render/FrameInputs.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"

#include "common/root_signature.hlsli"

namespace engine::render {
    namespace {
        // The one place that turns the compiler's accesses into D3D12 resource states
        [[nodiscard]] D3D12_RESOURCE_STATES ToState(fg::Access access) {
            switch (access) {
            case fg::Access::Present:
                return D3D12_RESOURCE_STATE_PRESENT;
            case fg::Access::ColorWrite:
                return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case fg::Access::DepthWrite:
                return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case fg::Access::ShaderRead:
                return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            }
            ENGINE_ASSERT_MSG(false, "Unknown access {}", std::to_underlying(access));
            return D3D12_RESOURCE_STATE_COMMON;
        }
    } // namespace

    void PassBuilder::WriteColor(FgTexture texture, std::optional<ClearColor> clear, uint32_t view) {
        ENGINE_ASSERT_MSG(view < ImportBinding{}.views.size(), "View {} does not exist", view);
        m_graph.AddUse(m_pass, texture, fg::Access::ColorWrite);
        m_graph.m_passes[m_pass].colors.push_back({.texture = texture, .view = view, .clear = clear});
    }

    void PassBuilder::WriteDepth(FgTexture texture, std::optional<float> clear) {
        ENGINE_ASSERT_MSG(!m_graph.m_passes[m_pass].depth, "Pass {} writes two depth textures",
                          m_graph.m_passes[m_pass].name);
        m_graph.AddUse(m_pass, texture, fg::Access::DepthWrite);
        m_graph.m_passes[m_pass].depth = FrameGraph::DepthTarget{.texture = texture, .clear = clear};
    }

    FgTexture FrameGraph::Import(std::string_view name, fg::Access start, fg::Access end) {
        ENGINE_ASSERT_MSG(!m_isCompiled && !m_executing, "Import of {} after Compile", name);
        m_textureNames.emplace_back(name);
        return {m_desc.AddImport(start, end)};
    }

    void FrameGraph::AddPass(std::string_view name, const std::function<void(PassBuilder &)> &setup,
                             ExecuteFn execute) {
        ENGINE_ASSERT_MSG(!m_isCompiled && !m_executing, "AddPass of {} after Compile", name);
        const auto passIndex = static_cast<uint32_t>(m_passes.size());
        m_passes.push_back({.name = std::string(name), .execute = std::move(execute), .colors = {}, .depth = {}});

        m_setupUses.clear();
        PassBuilder builder(*this, passIndex);
        setup(builder);
        ENGINE_ASSERT_MSG(m_passes.back().colors.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
                          "Pass {} writes more than 8 colour targets", name);
        [[maybe_unused]] const uint32_t descPass = m_desc.AddPass(m_setupUses);
        ENGINE_ASSERT(descPass == passIndex);
    }

    void FrameGraph::AddUse([[maybe_unused]] uint32_t pass, FgTexture texture, fg::Access access) {
        ENGINE_ASSERT_MSG(texture.index < m_textureNames.size(), "Pass {} uses a texture of another graph",
                          m_passes[pass].name);
        m_setupUses.push_back({.texture = texture.index, .access = access});
    }

    void FrameGraph::Compile() {
        ENGINE_ASSERT_MSG(!m_isCompiled, "Compile called twice without Reset");
        m_compiled = fg::Compile(m_desc);
        m_gpuBindings.assign(m_compiled.gpuTextures.size(), ImportBinding{});
        m_isCompiled = true;

#ifdef _DEBUG
        for (size_t pass = 0; pass < m_passes.size(); ++pass) {
            if (!m_compiled.passes[pass].kept) {
                core::LogInfo("Frame graph: pass {} is skipped, nothing uses what it writes", m_passes[pass].name);
            }
        }
#endif
    }

    void FrameGraph::Execute(rhi::CommandContext &commands, rhi::FrameContext &frame,
                             std::span<const ImportBinding> imports, const FrameInputs &inputs) {
        ENGINE_ASSERT_MSG(m_isCompiled, "Execute before Compile");
        m_executing = true;

        uint32_t importCount = 0;
        for (const fg::GraphDesc::Texture &texture : m_desc.textures) {
            importCount += texture.imported ? 1 : 0;
        }
        ENGINE_ASSERT_MSG(imports.size() == importCount, "Execute got {} imported textures, the graph has {}",
                          imports.size(), importCount);
        for (const ImportBinding &binding : imports) {
            ENGINE_ASSERT_MSG(binding.texture.index < m_desc.textures.size() &&
                                  m_desc.textures[binding.texture.index].imported,
                              "Execute got a binding for a texture that was not imported");
            m_gpuBindings[m_compiled.textures[binding.texture.index].gpuTexture] = binding;
        }

        for (uint32_t passIndex = 0; passIndex < m_passes.size(); ++passIndex) {
            if (!m_compiled.passes[passIndex].kept) {
                continue;
            }
            const Pass &pass = m_passes[passIndex];

            for (const fg::Transition &transition : m_compiled.TransitionsBefore(passIndex)) {
                commands.Transition(m_gpuBindings[transition.gpuTexture].resource, ToState(transition.before),
                                    ToState(transition.after));
            }
            // A callback may record straight into the native command list (ImGui does), past the batched barriers
            commands.FlushBarriers();

            // TODO: ScopedGpuTimer(commands, profiler, pass.name) once the GPU profiler exists
            BindTargetsAndClear(pass, commands);
            // Setting a root signature clears every root argument, so the frame constants are bound again for each
            // pass. Each pass starts from this state, whatever the previous one set (ImGui sets its own).
            commands.SetGraphicsRootSignature(m_rootSignature);
            commands.SetGraphicsRootConstantBufferView(gpu::kRootFrameConstants, inputs.frameConstants);

            PassContext context(commands, frame, inputs);
            pass.execute(context);
        }

        for (const fg::Transition &transition : m_compiled.finalTransitions) {
            commands.Transition(m_gpuBindings[transition.gpuTexture].resource, ToState(transition.before),
                                ToState(transition.after));
        }
        m_executing = false;
    }

    void FrameGraph::Reset() {
        ENGINE_ASSERT_MSG(!m_executing, "Reset during Execute");
        m_desc.Clear();
        m_textureNames.clear();
        m_passes.clear();
        m_compiled = {};
        m_gpuBindings.clear();
        m_isCompiled = false;
    }

    void FrameGraph::BindTargetsAndClear(const Pass &pass, rhi::CommandContext &commands) const {
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs{};
        uint32_t width = 0;
        uint32_t height = 0;
        const auto matchSize = [&](const ImportBinding &binding) {
            ENGINE_ASSERT_MSG(width == 0 || (binding.width == width && binding.height == height),
                              "Pass {} has targets of different sizes", pass.name);
            width = binding.width;
            height = binding.height;
        };

        for (size_t i = 0; i < pass.colors.size(); ++i) {
            const ColorTarget &target = pass.colors[i];
            const ImportBinding &binding = BindingOf(target.texture);
            rtvs[i] = binding.views[target.view];
            matchSize(binding);
            if (target.clear) {
                commands.ClearRenderTarget(rtvs[i], *target.clear);
            }
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        if (pass.depth) {
            const ImportBinding &binding = BindingOf(pass.depth->texture);
            dsv = binding.views[0];
            matchSize(binding);
            if (pass.depth->clear) {
                commands.ClearDepth(dsv, *pass.depth->clear);
            }
        }

        commands.SetRenderTargets(std::span(rtvs.data(), pass.colors.size()), pass.depth ? &dsv : nullptr);
        if (width != 0) {
            commands.SetViewportAndScissor(width, height);
        }
    }

    const ImportBinding &FrameGraph::BindingOf(FgTexture texture) const {
        return m_gpuBindings[m_compiled.textures[texture.index].gpuTexture];
    }
} // namespace engine::render
