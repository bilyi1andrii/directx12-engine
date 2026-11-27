#include "render/FrameGraph.h"

#include <utility>

#include "core/Assert.h"
#include "core/Log.h"
#include "render/FrameInputs.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/GpuProfiler.h"

#include "common/root_signature.hlsli"

namespace render {
    namespace {
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
            return D3D12_RESOURCE_STATE_COMMON;
        }
    } // namespace

    void PassBuilder::WriteColor(FgTexture texture, std::optional<ClearColor> clear, uint32_t view) {
        m_graph.AddUse(texture, fg::Access::ColorWrite);
        m_graph.m_passes[m_pass].colors.push_back({.texture = texture, .view = view, .clear = clear});
    }

    void PassBuilder::WriteDepth(FgTexture texture, std::optional<float> clear) {
        ENGINE_ASSERT_MSG(!m_graph.m_passes[m_pass].depth, "Pass {} writes two depth textures",
                          m_graph.m_passes[m_pass].name);
        m_graph.AddUse(texture, fg::Access::DepthWrite);
        m_graph.m_passes[m_pass].depth = FrameGraph::DepthTarget{.texture = texture, .clear = clear};
    }

    void PassBuilder::Read(FgTexture texture) {
        m_graph.AddUse(texture, fg::Access::ShaderRead);
        [[maybe_unused]] const TransientTextureDesc *desc = m_graph.TransientDescOf(texture);
        ENGINE_ASSERT_MSG(desc != nullptr && desc->shaderRead,
                          "Pass {} reads texture {}, which has no shader view (a transient with shaderRead)",
                          m_graph.m_passes[m_pass].name, m_graph.m_textureNames[texture.index]);
    }

    uint32_t PassContext::ShaderIndex(FgTexture texture) const {
        return m_graph.m_gpuShaderIndices[m_graph.m_compiled.textures[texture.index].gpuTexture];
    }

    FgTexture FrameGraph::Import(std::string_view name, fg::Access start, fg::Access end) {
        m_textureNames.emplace_back(name);
        return {m_desc.AddImport(start, end)};
    }

    FgTexture FrameGraph::CreateTexture(std::string_view name, const TransientTextureDesc &desc) {
        uint32_t descId = 0;
        // one id per distinct desc
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
        const auto passIndex = static_cast<uint32_t>(m_passes.size());
        m_passes.push_back({.name = std::string(name),
                            .timerId = m_profiler.RegisterScope(name),
                            .execute = std::move(execute),
                            .colors = {},
                            .depth = {}});

        m_setupUses.clear();
        PassBuilder builder(*this, passIndex);
        setup(builder); // records uses into m_setupUses
        m_desc.AddPass(m_setupUses);
    }

    void FrameGraph::AddUse(FgTexture texture, fg::Access access) {
        m_setupUses.push_back({.texture = texture.index, .access = access});
    }

    void FrameGraph::Compile(TransientTexturePool &pool, rhi::FrameContext &frame) {
        m_compiled = fg::Compile(m_desc);
        m_gpuBindings.assign(m_compiled.gpuTextures.size(), TextureBinding{});
        m_gpuShaderIndices.assign(m_compiled.gpuTextures.size(), TransientTexturePool::kNoSrv);
        AcquireTransientTextures(pool, frame);

#ifdef _DEBUG
        for (size_t pass = 0; pass < m_passes.size(); ++pass) {
            if (!m_compiled.passes[pass].kept) {
                core::LogInfo("Frame graph: pass {} is skipped, nothing uses what it writes", m_passes[pass].name);
            }
        }
#endif
    }

    void FrameGraph::Execute(rhi::CommandContext &commands, rhi::FrameContext &frame,
                             std::span<const TextureBinding> imports, const FrameInputs &inputs) {
        for (const TextureBinding &binding : imports) {
            m_gpuBindings[m_compiled.textures[binding.texture.index].gpuTexture] = binding;
        }

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
            commands.FlushBarriers();

            const rhi::ScopedGpuEvent event(commands, pass.name.c_str());
            const rhi::ScopedGpuTimer timer(commands, m_profiler, pass.timerId);
            BindTargetsAndClear(pass, commands);
            // per pass, ImGui sets its own
            commands.SetGraphicsRootSignature(m_rootSignature);
            commands.SetGraphicsRootConstantBufferView(gpu::kRootFrameConstants, inputs.frameConstants);

            PassContext context(*this, commands, frame, inputs);
            pass.execute(context);
        }

        for (const fg::Transition &transition : m_compiled.finalTransitions) {
            commands.Transition(m_gpuBindings[transition.gpuTexture].resource, ToState(transition.before),
                                ToState(transition.after));
        }
    }

    void FrameGraph::Reset() {
        m_desc.Clear();
        m_textureNames.clear();
        m_transientDescs.clear();
        m_passes.clear();
        m_compiled = {};
        m_gpuBindings.clear();
        m_gpuShaderIndices.clear();
    }

    void FrameGraph::AcquireTransientTextures(TransientTexturePool &pool, rhi::FrameContext &frame) {
        std::vector<std::string_view> names(m_compiled.gpuTextures.size());
        for (size_t i = 0; i < m_compiled.textures.size(); ++i) {
            const uint32_t gpu = m_compiled.textures[i].gpuTexture;
            if (gpu != fg::kNoIndex && names[gpu].empty()) {
                names[gpu] = m_textureNames[i];
            }
        }

        std::vector<std::optional<ClearColor>> clearColors(m_compiled.gpuTextures.size());
        for (size_t pass = 0; pass < m_passes.size(); ++pass) {
            if (!m_compiled.passes[pass].kept) {
                continue;
            }
            for (const ColorTarget &target : m_passes[pass].colors) {
                const uint32_t gpu = m_compiled.textures[target.texture.index].gpuTexture;
                if (target.clear && !clearColors[gpu]) { // first clear colour wins
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
        pool.Acquire(m_poolRequests, frame, m_poolTextures);

        m_stateFixes.clear();
        size_t acquired = 0;
        for (size_t gpu = 0; gpu < m_compiled.gpuTextures.size(); ++gpu) {
            if (m_compiled.gpuTextures[gpu].imported) {
                continue;
            }
            const TransientTexturePool::Texture &texture = m_poolTextures[acquired];
            const D3D12_RESOURCE_STATES state = m_poolRequests[acquired].state;
            ++acquired;
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
        const auto matchSize = [&](const TextureBinding &binding) {
            ENGINE_ASSERT_MSG(width == 0 || (binding.width == width && binding.height == height),
                              "Pass {} has targets of different sizes", pass.name);
            width = binding.width;
            height = binding.height;
        };

        for (size_t i = 0; i < pass.colors.size(); ++i) {
            const ColorTarget &target = pass.colors[i];
            const TextureBinding &binding = BindingOf(target.texture);
            rtvs[i] = binding.views[target.view];
            matchSize(binding);
            if (target.clear) {
                commands.ClearRenderTarget(rtvs[i], *target.clear);
            }
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        if (pass.depth) {
            const TextureBinding &binding = BindingOf(pass.depth->texture);
            dsv = binding.views[0];
            matchSize(binding);
            if (pass.depth->clear) {
                commands.ClearDepth(dsv, *pass.depth->clear);
            }
        }

        commands.SetRenderTargets(std::span(rtvs.data(), pass.colors.size()), pass.depth ? &dsv : nullptr);
        if (width != 0) { // no targets, no viewport
            commands.SetViewportAndScissor(width, height);
        }
    }

    const TextureBinding &FrameGraph::BindingOf(FgTexture texture) const {
        return m_gpuBindings[m_compiled.textures[texture.index].gpuTexture];
    }

    const TransientTextureDesc *FrameGraph::TransientDescOf(FgTexture texture) const {
        const fg::GraphDesc::Texture &desc = m_desc.textures[texture.index];
        return desc.imported ? nullptr : &m_transientDescs[desc.descId];
    }
} // namespace render
