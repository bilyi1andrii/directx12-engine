#ifndef D3D12_ENGINE_FRAMEGRAPHCOMPILER_H
#define D3D12_ENGINE_FRAMEGRAPHCOMPILER_H

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

// The CPU-only part of the frame graph: from the passes and the textures they use, it works out which passes run,
// which transient textures can share one GPU texture, and the state changes (barriers) between passes.
// It knows no D3D12 types: FrameGraph turns its result into D3D12 calls.
namespace render::fg {
    // How a pass uses a texture. FrameGraph maps each value to one D3D12 resource state.
    enum class Access : uint8_t {
        Present,    // shown on screen; only as the start or end access of an imported texture
        ColorWrite, // render target
        DepthWrite, // depth target (depth test and depth write)
        ShaderRead, // sampled by a pixel shader
    };

    [[nodiscard]] constexpr bool IsWrite(Access access) noexcept {
        return access == Access::ColorWrite || access == Access::DepthWrite;
    }

    // Marks an index that does not exist: a texture no running pass uses has no first pass and no GPU texture
    inline constexpr uint32_t kNoIndex = std::numeric_limits<uint32_t>::max();

    // One use of a texture by a pass
    struct TextureUse {
        uint32_t texture = 0; // index returned by GraphDesc::AddImport / AddTransient
        Access access = Access::ShaderRead;
    };

    // The passes of a frame, in the order they run, and the textures they use.
    //
    // A texture is either imported or transient. An imported texture lives outside the graph (the back buffer):
    // it is in its start access when the frame begins and must be in its end access when the frame ends.
    // A transient texture lives only inside the frame, and the graph creates it. Transients with the same
    // description id have the same size, format and usage, so they can share one GPU texture.
    struct GraphDesc {
        struct Texture {
            bool imported = false;
            Access start = Access::Present; // imported only
            Access end = Access::Present;   // imported only
            uint32_t descId = 0;            // transient only
        };
        struct Pass {
            uint32_t firstUse = 0; // index into `uses`
            uint32_t useCount = 0;
        };

        std::vector<Texture> textures;
        std::vector<Pass> passes;
        std::vector<TextureUse> uses; // the uses of all passes, pass after pass

        // Each returns the new texture's index
        uint32_t AddImport(Access start, Access end);
        uint32_t AddTransient(uint32_t descId);
        // Adds a pass that runs after all passes added before it, and returns its index. A pass uses each
        // texture at most once, with a ColorWrite, DepthWrite or ShaderRead access.
        uint32_t AddPass(std::span<const TextureUse> passUses);

        [[nodiscard]] std::span<const TextureUse> UsesOf(uint32_t pass) const {
            return std::span(uses).subspan(passes[pass].firstUse, passes[pass].useCount);
        }
        void Clear() noexcept;
    };

    // A state change of one GPU texture
    struct Transition {
        uint32_t gpuTexture = 0; // index into CompiledGraph::gpuTextures
        Access before = Access::Present;
        Access after = Access::Present;

        friend bool operator==(const Transition &, const Transition &) = default;
    };

    // What Compile works out. Indices of passes and textures are those of the GraphDesc.
    struct CompiledGraph {
        struct Pass {
            bool kept = false;            // false: nothing uses what the pass writes, so it does not run
            uint32_t firstTransition = 0; // into `transitions`
            uint32_t transitionCount = 0;
        };
        struct Texture {
            uint32_t firstPass = kNoIndex; // first and last kept pass that uses it
            uint32_t lastPass = kNoIndex;
            uint32_t gpuTexture = kNoIndex; // into `gpuTextures`; kNoIndex for a transient no kept pass uses
        };
        // One real texture: an imported one, or one shared by transients whose lifetimes do not overlap
        struct GpuTexture {
            bool imported = false;
            uint32_t descId = 0; // transient only
            // Transient only: the state it is left in at the end of a frame, which is also its state when the
            // next frame starts. A new texture must be created in this state.
            Access restAccess = Access::Present;
        };

        std::vector<Pass> passes;
        std::vector<Texture> textures;
        std::vector<GpuTexture> gpuTextures;
        std::vector<Transition> transitions; // those before each kept pass, pass after pass
        // After the last pass: imported textures back to their end access
        std::vector<Transition> finalTransitions;

        // The transitions to make before `pass` runs
        [[nodiscard]] std::span<const Transition> TransitionsBefore(uint32_t pass) const {
            return std::span(transitions).subspan(passes[pass].firstTransition, passes[pass].transitionCount);
        }
    };

    // Compiles `graph`. Passes keep their order. A pass runs only if it writes an imported texture or a texture
    // that a later running pass uses. Reading a transient that no earlier pass wrote is a bug (asserted).
    [[nodiscard]] CompiledGraph Compile(const GraphDesc &graph);
} // namespace render::fg

#endif // D3D12_ENGINE_FRAMEGRAPHCOMPILER_H
