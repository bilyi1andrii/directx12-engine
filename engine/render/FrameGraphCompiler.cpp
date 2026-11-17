#include "render/FrameGraphCompiler.h"

#include <cstddef>

#include "core/Assert.h"

namespace engine::render::fg {
    uint32_t GraphDesc::AddImport(Access start, Access end) {
        textures.push_back({.imported = true, .start = start, .end = end, .descId = 0});
        return static_cast<uint32_t>(textures.size() - 1);
    }

    uint32_t GraphDesc::AddTransient(uint32_t descId) {
        textures.push_back({.imported = false, .start = Access::Present, .end = Access::Present, .descId = descId});
        return static_cast<uint32_t>(textures.size() - 1);
    }

    uint32_t GraphDesc::AddPass(std::span<const TextureUse> passUses) {
        for (size_t i = 0; i < passUses.size(); ++i) {
            ENGINE_ASSERT_MSG(passUses[i].texture < textures.size(), "Pass {} uses texture {}, which does not exist",
                              passes.size(), passUses[i].texture);
            ENGINE_ASSERT_MSG(passUses[i].access != Access::Present, "Pass {} uses texture {} with Access::Present",
                              passes.size(), passUses[i].texture);
            for (size_t j = 0; j < i; ++j) {
                ENGINE_ASSERT_MSG(passUses[j].texture != passUses[i].texture, "Pass {} uses texture {} twice",
                                  passes.size(), passUses[i].texture);
            }
        }
        passes.push_back(
            {.firstUse = static_cast<uint32_t>(uses.size()), .useCount = static_cast<uint32_t>(passUses.size())});
        uses.insert(uses.end(), passUses.begin(), passUses.end());
        return static_cast<uint32_t>(passes.size() - 1);
    }

    void GraphDesc::Clear() noexcept {
        textures.clear();
        passes.clear();
        uses.clear();
    }

    namespace {
        // Walks the passes from the last one back, so whether a later pass runs is known before an earlier one is
        // decided. A pass runs if it writes a texture that is needed. Imported textures are always needed, and
        // every texture a running pass uses becomes needed by the passes before it. A write counts as a use too:
        // a pass may keep the earlier contents of a texture it writes (no clear).
        void CullPasses(const GraphDesc &graph, CompiledGraph &out) {
            std::vector<bool> needed(graph.textures.size());
            for (size_t i = 0; i < graph.textures.size(); ++i) {
                needed[i] = graph.textures[i].imported;
            }
            for (size_t pass = graph.passes.size(); pass-- > 0;) {
                bool kept = false;
                for (const TextureUse &use : graph.UsesOf(static_cast<uint32_t>(pass))) {
                    kept = kept || (IsWrite(use.access) && needed[use.texture]);
                }
                out.passes[pass].kept = kept;
                if (kept) {
                    for (const TextureUse &use : graph.UsesOf(static_cast<uint32_t>(pass))) {
                        needed[use.texture] = true;
                    }
                }
            }
        }

        // The first and last running pass of each texture
        void FindLifetimes(const GraphDesc &graph, CompiledGraph &out) {
            for (uint32_t pass = 0; pass < graph.passes.size(); ++pass) {
                if (!out.passes[pass].kept) {
                    continue;
                }
                for (const TextureUse &use : graph.UsesOf(pass)) {
                    CompiledGraph::Texture &texture = out.textures[use.texture];
                    if (texture.firstPass == kNoIndex) {
                        texture.firstPass = pass;
                    }
                    texture.lastPass = pass;
                }
            }
        }

        // Every imported texture gets its own GPU texture. A transient takes the first GPU texture with the same
        // description whose last user ran before the transient's first pass; otherwise it gets a new one.
        void AssignGpuTextures(const GraphDesc &graph, CompiledGraph &out) {
            for (size_t i = 0; i < graph.textures.size(); ++i) {
                if (graph.textures[i].imported) {
                    out.textures[i].gpuTexture = static_cast<uint32_t>(out.gpuTextures.size());
                    out.gpuTextures.push_back({.imported = true, .descId = 0, .restAccess = Access::Present});
                }
            }

            // The last pass that uses each GPU texture so far; only read for transients
            std::vector<uint32_t> busyUntil(out.gpuTextures.size(), kNoIndex);
            // In pass order, so each transient is assigned at its first pass, after the ones that start earlier
            for (uint32_t pass = 0; pass < graph.passes.size(); ++pass) {
                if (!out.passes[pass].kept) {
                    continue;
                }
                for (const TextureUse &use : graph.UsesOf(pass)) {
                    const GraphDesc::Texture &desc = graph.textures[use.texture];
                    CompiledGraph::Texture &texture = out.textures[use.texture];
                    if (desc.imported || texture.firstPass != pass) {
                        continue;
                    }
                    for (uint32_t gpu = 0; gpu < out.gpuTextures.size(); ++gpu) {
                        const CompiledGraph::GpuTexture &candidate = out.gpuTextures[gpu];
                        if (!candidate.imported && candidate.descId == desc.descId && busyUntil[gpu] < pass) {
                            texture.gpuTexture = gpu;
                            break;
                        }
                    }
                    if (texture.gpuTexture == kNoIndex) {
                        texture.gpuTexture = static_cast<uint32_t>(out.gpuTextures.size());
                        out.gpuTextures.push_back(
                            {.imported = false, .descId = desc.descId, .restAccess = Access::Present});
                        busyUntil.push_back(kNoIndex);
                    }
                    busyUntil[texture.gpuTexture] = texture.lastPass;
                }
            }
        }

        // Follows the state of each GPU texture through the running passes and records every change.
        // Resource states persist between frames on one queue, so a transient starts the frame in the state its
        // last use left it in: it needs no transition at the end of the frame.
        void FindTransitions(const GraphDesc &graph, CompiledGraph &out) {
            std::vector<Access> state(out.gpuTextures.size(), Access::Present);
            for (uint32_t pass = 0; pass < graph.passes.size(); ++pass) {
                if (out.passes[pass].kept) {
                    for (const TextureUse &use : graph.UsesOf(pass)) {
                        state[out.textures[use.texture].gpuTexture] = use.access;
                    }
                }
            }
            for (size_t gpu = 0; gpu < out.gpuTextures.size(); ++gpu) {
                if (!out.gpuTextures[gpu].imported) {
                    out.gpuTextures[gpu].restAccess = state[gpu];
                }
            }
            for (size_t i = 0; i < graph.textures.size(); ++i) {
                if (graph.textures[i].imported) {
                    state[out.textures[i].gpuTexture] = graph.textures[i].start;
                }
            }

            for (uint32_t pass = 0; pass < graph.passes.size(); ++pass) {
                CompiledGraph::Pass &compiled = out.passes[pass];
                compiled.firstTransition = static_cast<uint32_t>(out.transitions.size());
                if (compiled.kept) {
                    for (const TextureUse &use : graph.UsesOf(pass)) {
                        const uint32_t gpu = out.textures[use.texture].gpuTexture;
                        if (state[gpu] != use.access) {
                            out.transitions.push_back({.gpuTexture = gpu, .before = state[gpu], .after = use.access});
                            state[gpu] = use.access;
                        }
                    }
                }
                compiled.transitionCount = static_cast<uint32_t>(out.transitions.size()) - compiled.firstTransition;
            }

            for (size_t i = 0; i < graph.textures.size(); ++i) {
                const GraphDesc::Texture &desc = graph.textures[i];
                const uint32_t gpu = out.textures[i].gpuTexture;
                if (desc.imported && state[gpu] != desc.end) {
                    out.finalTransitions.push_back({.gpuTexture = gpu, .before = state[gpu], .after = desc.end});
                }
            }
        }

        // Debug check, in declaration order: a pass that reads a transient must come after a pass that writes it
        void AssertTransientsWrittenBeforeRead([[maybe_unused]] const GraphDesc &graph) {
#ifdef _DEBUG
            std::vector<bool> written(graph.textures.size());
            for (uint32_t pass = 0; pass < graph.passes.size(); ++pass) {
                for (const TextureUse &use : graph.UsesOf(pass)) {
                    ENGINE_ASSERT_MSG(
                        graph.textures[use.texture].imported || IsWrite(use.access) || written[use.texture],
                        "Pass {} reads transient texture {} before any pass writes it", pass, use.texture);
                    written[use.texture] = written[use.texture] || IsWrite(use.access);
                }
            }
#endif
        }
    } // namespace

    CompiledGraph Compile(const GraphDesc &graph) {
        AssertTransientsWrittenBeforeRead(graph);

        CompiledGraph out;
        out.passes.resize(graph.passes.size());
        out.textures.resize(graph.textures.size());
        CullPasses(graph, out);
        FindLifetimes(graph, out);
        AssignGpuTextures(graph, out);
        FindTransitions(graph, out);
        return out;
    }
} // namespace engine::render::fg
