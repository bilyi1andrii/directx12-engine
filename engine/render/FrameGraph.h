#ifndef D3D12_ENGINE_FRAMEGRAPH_H
#define D3D12_ENGINE_FRAMEGRAPH_H

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "render/FrameGraphCompiler.h"
#include "render/TransientTexturePool.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class CommandContext;
    class FrameContext;
} // namespace engine::rhi

namespace engine::render {
    struct FrameInputs;
    class FrameGraph;

    // A texture of a FrameGraph. Valid for the graph that returned it, until its next Reset.
    struct FgTexture {
        uint32_t index = fg::kNoIndex;

        [[nodiscard]] bool IsValid() const noexcept { return index != fg::kNoIndex; }
    };

    // An imported texture's resource and views for one frame. The back buffer changes every frame, so Execute
    // takes these anew each time; the graph only keeps the texture's states.
    struct ImportBinding {
        FgTexture texture;
        ID3D12Resource *resource = nullptr;
        // Render-target views of the resource, picked by PassBuilder::WriteColor's `view`; for a depth texture,
        // its depth-stencil view at [0]
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> views{};
        uint32_t width = 0;
        uint32_t height = 0;
    };

    using ClearColor = std::array<float, 4>;

    // Given to a pass's setup callback to declare the textures the pass uses. The graph binds what is declared
    // here before the pass runs, and does the clears.
    class PassBuilder {
    public:
        // Draws into `texture`. With `clear`, the graph clears it first; std::nullopt keeps the contents.
        // `view` picks one of the texture's views (ImportBinding::views).
        void WriteColor(FgTexture texture, std::optional<ClearColor> clear, uint32_t view = 0);
        // Depth test and depth write into `texture`. With `clear`, the graph clears it to that depth first.
        void WriteDepth(FgTexture texture, std::optional<float> clear);
        // Samples `texture` in pixel shaders; the pass gets its bindless index from PassContext::ShaderIndex.
        // Only a transient created with `shaderRead`, written by an earlier pass.
        void Read(FgTexture texture);

    private:
        friend class FrameGraph;
        PassBuilder(FrameGraph &graph, uint32_t pass) : m_graph(graph), m_pass(pass) {}

        FrameGraph &m_graph;
        uint32_t m_pass;
    };

    // Given to a pass's execute callback. The callback sets PSOs and draws; targets, viewport, root signature and
    // frame constants (b1) are already bound.
    class PassContext {
    public:
        [[nodiscard]] rhi::CommandContext &Commands() const noexcept { return m_commands; }
        // The frame slot being recorded, for resources freed during the pass
        [[nodiscard]] rhi::FrameContext &Frame() const noexcept { return m_frame; }
        [[nodiscard]] const FrameInputs &Inputs() const noexcept { return m_inputs; }
        // The bindless index of a texture this pass declared with PassBuilder::Read, for shaders to sample it
        // (ResourceDescriptorHeap[index]). Stays the same until the graph is rebuilt.
        [[nodiscard]] uint32_t ShaderIndex(FgTexture texture) const;

    private:
        friend class FrameGraph;
        PassContext(const FrameGraph &graph, uint32_t pass, rhi::CommandContext &commands, rhi::FrameContext &frame,
                    const FrameInputs &inputs)
            : m_graph(graph), m_pass(pass), m_commands(commands), m_frame(frame), m_inputs(inputs) {}

        const FrameGraph &m_graph;
        uint32_t m_pass;
        rhi::CommandContext &m_commands;
        rhi::FrameContext &m_frame;
        const FrameInputs &m_inputs;
    };

    // The frame as a list of passes that declare which textures they read and write. From those declarations the
    // graph works out the barriers between passes and which passes can be skipped, then records the passes in the
    // order they were added.
    //
    // Built once and executed every frame: Import, CreateTexture and AddPass, then Compile, then Execute once per
    // frame. Reset and build again when the passes or texture sizes change. Execute callbacks are kept until Reset,
    // so they must not capture data of one frame; they read it through PassContext::Inputs().
    //
    // Not copyable or movable: pass callbacks may keep references to it.
    class FrameGraph {
    public:
        using ExecuteFn = std::function<void(PassContext &)>;

        // `rootSignature` is bound before every pass and must outlive the graph
        explicit FrameGraph(ID3D12RootSignature *rootSignature) : m_rootSignature(rootSignature) {}

        FrameGraph(const FrameGraph &) = delete;
        FrameGraph &operator=(const FrameGraph &) = delete;
        FrameGraph(FrameGraph &&) = delete;
        FrameGraph &operator=(FrameGraph &&) = delete;

        // A texture that lives outside the graph. It is in `start` when the frame begins, and the graph leaves it in
        // `end`. Its resource is given to every Execute.
        [[nodiscard]] FgTexture Import(std::string_view name, fg::Access start, fg::Access end);

        // A texture that lives only inside the frame. Compile acquires it from the pool; transients whose passes never
        // overlap may share one texture. Its contents are undefined until a pass writes or clears it.
        // `name` is ASCII and names the GPU texture in debuggers ("FG/<name>").
        [[nodiscard]] FgTexture CreateTexture(std::string_view name, const TransientTextureDesc &desc);

        // Adds a pass that runs after the ones added before it. `setup` runs at once and declares the textures;
        // `execute` runs in every Execute, unless nothing uses what the pass writes.
        void AddPass(std::string_view name, const std::function<void(PassBuilder &)> &setup, ExecuteFn execute);

        // Works out which passes run and the barriers, and acquires the transient textures from `pool`. The pool's
        // textures this graph does not use are destroyed once the GPU has finished `frame`. Call once after the
        // last AddPass, inside a frame.
        void Compile(TransientTexturePool &pool, rhi::FrameContext &frame);
        [[nodiscard]] bool IsCompiled() const noexcept { return m_isCompiled; }

        // Records the passes into `commands`, with this frame's imported textures (one binding per Import) and
        // inputs. Ends with every imported texture in its end state, and every transient in the state the next
        // frame starts from.
        void Execute(rhi::CommandContext &commands, rhi::FrameContext &frame, std::span<const ImportBinding> imports,
                     const FrameInputs &inputs);

        // Removes every pass and texture; the graph can be built again. After a Compile, Execute must run once before
        // Reset (asserted): the first Execute moves pooled textures into the states the pool now reports.
        void Reset();

    private:
        friend class PassBuilder;
        friend class PassContext;

        struct ColorTarget {
            FgTexture texture;
            uint32_t view = 0;
            std::optional<ClearColor> clear;
        };
        struct DepthTarget {
            FgTexture texture;
            std::optional<float> clear;
        };
        struct Pass {
            std::string name;
            ExecuteFn execute;
            std::vector<ColorTarget> colors;
            std::optional<DepthTarget> depth;
        };
        // A pooled texture that is not yet in the state the compiled graph expects at the start of a frame
        struct StateFix {
            ID3D12Resource *resource = nullptr;
            D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_COMMON;
        };

        // Acquires a GPU texture from `pool` for each transient GPU texture of m_compiled
        void AcquireTransientTextures(TransientTexturePool &pool, rhi::FrameContext &frame);

        // For PassBuilder
        void AddUse(uint32_t pass, FgTexture texture, fg::Access access);

        void BindTargetsAndClear(const Pass &pass, rhi::CommandContext &commands) const;
        [[nodiscard]] const ImportBinding &BindingOf(FgTexture texture) const;

        ID3D12RootSignature *m_rootSignature;
        fg::GraphDesc m_desc;
        std::vector<std::string> m_textureNames; // by FgTexture::index
        // Each different description of a transient, once; fg::GraphDesc::Texture::descId indexes it
        std::vector<TransientTextureDesc> m_transientDescs;
        std::vector<Pass> m_passes;
        std::vector<fg::TextureUse> m_setupUses; // the uses of the pass being set up
        fg::CompiledGraph m_compiled;
        bool m_isCompiled = false;
        bool m_executing = false;
        // The binding of each GPU texture (fg::CompiledGraph::gpuTextures): transients' from Compile, imports' from
        // each Execute
        std::vector<ImportBinding> m_gpuBindings;
        // The bindless index of each GPU texture's shader view; TransientTexturePool::kNoSrv if it has none
        std::vector<uint32_t> m_gpuShaderIndices;
        // Made at the start of the first Execute after Compile, then cleared
        std::vector<StateFix> m_stateFixes;
        // Scratch for AcquireTransientTextures
        std::vector<TransientTexturePool::Request> m_poolRequests;
        std::vector<TransientTexturePool::Texture> m_poolTextures;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_FRAMEGRAPH_H
