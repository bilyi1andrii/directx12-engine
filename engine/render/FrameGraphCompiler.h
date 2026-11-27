#ifndef D3D12_ENGINE_FRAMEGRAPHCOMPILER_H
#define D3D12_ENGINE_FRAMEGRAPHCOMPILER_H

// The CPU-only part of the frame graph: from the passes and their textures it works out which passes run, which
// textures can share memory, and every state change between passes.

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace render::fg {
    // how pass uses texture, also state it needs
    enum class Access : uint8_t {
        Present,
        ColorWrite,
        DepthWrite,
        ShaderRead,
    };

    [[nodiscard]] constexpr bool IsWrite(Access access) noexcept {
        return access == Access::ColorWrite || access == Access::DepthWrite;
    }

    inline constexpr uint32_t kNoIndex = std::numeric_limits<uint32_t>::max(); // unused or not assigned

    // one line of pass declaration
    struct TextureUse {
        uint32_t texture = 0;
        Access access = Access::ShaderRead;
    };

    struct GraphDesc {
        struct Texture {
            bool imported = false;          // owned outside graph, e.g. back buffer
            Access start = Access::Present; // state import arrives in
            Access end = Access::Present;   // state import must leave in
            uint32_t descId = 0;            // size and format, same id means interchangeable
        };
        struct Pass {
            uint32_t firstUse = 0; // position in uses
            uint32_t useCount = 0;
        };

        std::vector<Texture> textures;
        std::vector<Pass> passes;
        std::vector<TextureUse> uses;

        uint32_t AddImport(Access start, Access end);
        uint32_t AddTransient(uint32_t descId);
        uint32_t AddPass(std::span<const TextureUse> passUses);

        [[nodiscard]] std::span<const TextureUse> UsesOf(uint32_t pass) const {
            return std::span(uses).subspan(passes[pass].firstUse, passes[pass].useCount);
        }
        void Clear() noexcept;
    };

    struct Transition {
        uint32_t gpuTexture = 0;
        Access before = Access::Present;
        Access after = Access::Present;

        friend bool operator==(const Transition &, const Transition &) = default;
    };

    struct CompiledGraph {
        struct Pass {
            bool kept = false;
            uint32_t firstTransition = 0; // position in transitions
            uint32_t transitionCount = 0;
        };
        // per graph texture
        struct Texture {
            uint32_t firstPass = kNoIndex; // first and last kept pass using it
            uint32_t lastPass = kNoIndex;
            uint32_t gpuTexture = kNoIndex; // real texture it maps to
        };
        // one real texture, several graph textures may share it
        struct GpuTexture {
            bool imported = false;
            uint32_t descId = 0;
            Access restAccess = Access::Present; // state at frame start and end, transients only
        };

        std::vector<Pass> passes;
        std::vector<Texture> textures;
        std::vector<GpuTexture> gpuTextures;
        std::vector<Transition> transitions;
        std::vector<Transition> finalTransitions; // imports back to end state

        [[nodiscard]] std::span<const Transition> TransitionsBefore(uint32_t pass) const {
            return std::span(transitions).subspan(passes[pass].firstTransition, passes[pass].transitionCount);
        }
    };

    [[nodiscard]] CompiledGraph Compile(const GraphDesc &graph);
} // namespace render::fg

#endif // D3D12_ENGINE_FRAMEGRAPHCOMPILER_H
