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

    void PassBuilder::Read(FgTexture texture) {
        m_graph.AddUse(m_pass, texture, fg::Access::ShaderRead);
        [[maybe_unused]] const fg::GraphDesc::Texture &desc = m_graph.m_desc.textures[texture.index];
        ENGINE_ASSERT_MSG(!desc.imported && m_graph.m_transientDescs[desc.descId].shaderRead,
                          "Pass {} reads texture {}, which has no shader view (a transient with shaderRead)",
                          m_graph.m_passes[m_pass].name, m_graph.m_textureNames[texture.index]);
    }

    uint32_t PassContext::ShaderIndex(FgTexture texture) const {
#ifdef _DEBUG
        bool declared = false;
        for (const fg::TextureUse &use : m_graph.m_desc.UsesOf(m_pass)) {
            declared = declared || (use.texture == texture.index && use.access == fg::Access::ShaderRead);
        }
        ENGINE_ASSERT_MSG(declared, "Pass {} samples texture {} without PassBuilder::Read",
                          m_graph.m_passes[m_pass].name, texture.index);
#endif
        return m_graph.m_gpuShaderIndices[m_graph.m_compiled.textures[texture.index].gpuTexture];
    }

    FgTexture FrameGraph::Import(std::string_view name, fg::Access start, fg::Access end) {
        ENGINE_ASSERT_MSG(!m_isCompiled && !m_executing, "Import of {} after Compile", name);
        m_textureNames.emplace_back(name);
        return {m_desc.AddImport(start, end)};
    }

    FgTexture FrameGraph::CreateTexture(std::string_view name, const TransientTextureDesc &desc) {
        ENGINE_ASSERT_MSG(!m_isCompiled && !m_executing, "CreateTexture of {} after Compile", name);
        uint32_t descId = 0;
        while (descId < m_transientDescs.size() && m_transientDescs[descId] != desc) {
            ++descId;
        }
        if (descId == m_transientDescs.size()) {
            m_transientDescs.push_back(desc);
        }
        m_textureNames.emplace_back(name);
        return {m_desc.AddTransient(descId)};
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

    void FrameGraph::Compile(TransientTexturePool &pool, rhi::FrameContext &frame) {
        ENGINE_ASSERT_MSG(!m_isCompiled, "Compile called twice without Reset");
        m_compiled = fg::Compile(m_desc);
        m_gpuBindings.assign(m_compiled.gpuTextures.size(), ImportBinding{});
        m_gpuShaderIndices.assign(m_compiled.gpuTextures.size(), TransientTexturePool::kNoSrv);
        TakeTransients(pool, frame);
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

        // Pooled textures that a previous graph left in another state. Flushed with the first pass's barriers.
        for (const StateFix &fix : m_stateFixes) {
            commands.Transition(fix.resource, fix.before, fix.after);
        }
        m_stateFixes.clear();

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

            PassContext context(*this, passIndex, commands, frame, inputs);
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
        m_transientDescs.clear();
        m_passes.clear();
        m_compiled = {};
        m_gpuBindings.clear();
        m_gpuShaderIndices.clear();
        // The pool already counts these textures in their new state, so the fixes must be made before a new graph
        ENGINE_ASSERT_MSG(m_stateFixes.empty(), "Reset between Compile and Execute");
        m_isCompiled = false;
    }

    void FrameGraph::TakeTransients(TransientTexturePool &pool, rhi::FrameContext &frame) {
        // The name of each transient GPU texture: that of the first graph texture that uses it
        std::vector<std::string_view> names(m_compiled.gpuTextures.size());
        for (size_t i = 0; i < m_compiled.textures.size(); ++i) {
            const uint32_t gpu = m_compiled.textures[i].gpuTexture;
            if (gpu != fg::kNoIndex && names[gpu].empty()) {
                names[gpu] = m_textureNames[i];
            }
        }

        // The colour the first running pass clears each GPU texture to: the pool creates it with that optimized
        // clear value. Transients that share a texture and clear it to different colours make the later clears
        // slower (the debug layer warns).
        std::vector<std::optional<ClearColor>> clearColors(m_compiled.gpuTextures.size());
        for (size_t pass = 0; pass < m_passes.size(); ++pass) {
            if (!m_compiled.passes[pass].kept) {
                continue;
            }
            for (const ColorTarget &target : m_passes[pass].colors) {
                const uint32_t gpu = m_compiled.textures[target.texture.index].gpuTexture;
                if (target.clear && !clearColors[gpu]) {
                    clearColors[gpu] = target.clear;
                }
            }
        }

        m_poolRequests.clear();
        for (size_t gpu = 0; gpu < m_compiled.gpuTextures.size(); ++gpu) {
            const fg::CompiledGraph::GpuTexture &texture = m_compiled.gpuTextures[gpu];
            if (!texture.imported) {
                m_poolRequests.push_back({.desc = m_transientDescs[texture.descId],
                                          .clearColor = clearColors[gpu],
                                          .state = ToState(texture.restAccess),
                                          .name = names[gpu]});
            }
        }
        pool.Take(m_poolRequests, frame, m_poolTextures);

        m_stateFixes.clear();
        size_t taken = 0;
        for (size_t gpu = 0; gpu < m_compiled.gpuTextures.size(); ++gpu) {
            if (m_compiled.gpuTextures[gpu].imported) {
                continue;
            }
            const TransientTexturePool::Texture &texture = m_poolTextures[taken];
            const D3D12_RESOURCE_STATES state = m_poolRequests[taken].state;
            ++taken;
            m_gpuBindings[gpu] = {.texture = {},
                                  .resource = texture.resource,
                                  .views = {texture.view},
                                  .width = texture.width,
                                  .height = texture.height};
            m_gpuShaderIndices[gpu] = texture.srvIndex;
            if (texture.currentState != state) {
                m_stateFixes.push_back({.resource = texture.resource, .before = texture.currentState, .after = state});
            }
        }
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
