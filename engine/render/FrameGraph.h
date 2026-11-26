#ifndef D3D12_ENGINE_FRAMEGRAPH_H
#define D3D12_ENGINE_FRAMEGRAPH_H

// The frame as a list of passes that say which textures they draw into and read. From that, the graph places every
// barrier, binds the targets, does the clears and skips passes whose output nobody uses.

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

namespace rhi {
    class CommandContext;
    class FrameContext;
    class GpuProfiler;
} // namespace rhi

namespace render {
    struct FrameInputs;
    class FrameGraph;

    struct FgTexture {
        uint32_t index = fg::kNoIndex;
    };

    struct TextureBinding {
        FgTexture texture;
        ID3D12Resource *resource = nullptr;
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> views{};
        uint32_t width = 0;
        uint32_t height = 0;
    };

    using ClearColor = std::array<float, 4>;

    class PassBuilder {
    public:
        void WriteColor(FgTexture texture, std::optional<ClearColor> clear, uint32_t view = 0);
        void WriteDepth(FgTexture texture, std::optional<float> clear);
        void Read(FgTexture texture);

    private:
        friend class FrameGraph;
        PassBuilder(FrameGraph &graph, uint32_t pass) : m_graph(graph), m_pass(pass) {}

        FrameGraph &m_graph;
        uint32_t m_pass;
    };

    class PassContext {
    public:
        [[nodiscard]] rhi::CommandContext &Commands() const noexcept { return m_commands; }
        [[nodiscard]] rhi::FrameContext &Frame() const noexcept { return m_frame; }
        [[nodiscard]] const FrameInputs &Inputs() const noexcept { return m_inputs; }
        [[nodiscard]] uint32_t ShaderIndex(FgTexture texture) const;

    private:
        friend class FrameGraph;
        PassContext(const FrameGraph &graph, rhi::CommandContext &commands, rhi::FrameContext &frame,
                    const FrameInputs &inputs)
            : m_graph(graph), m_commands(commands), m_frame(frame), m_inputs(inputs) {}

        const FrameGraph &m_graph;
        rhi::CommandContext &m_commands;
        rhi::FrameContext &m_frame;
        const FrameInputs &m_inputs;
    };

    class FrameGraph {
    public:
        using ExecuteFn = std::function<void(PassContext &)>;

        FrameGraph(ID3D12RootSignature *rootSignature, rhi::GpuProfiler &profiler)
            : m_rootSignature(rootSignature), m_profiler(profiler) {}

        FrameGraph(const FrameGraph &) = delete;
        FrameGraph &operator=(const FrameGraph &) = delete;
        FrameGraph(FrameGraph &&) = delete;
        FrameGraph &operator=(FrameGraph &&) = delete;

        [[nodiscard]] FgTexture Import(std::string_view name, fg::Access start, fg::Access end);

        [[nodiscard]] FgTexture CreateTexture(std::string_view name, const TransientTextureDesc &desc);

        void AddPass(std::string_view name, const std::function<void(PassBuilder &)> &setup, ExecuteFn execute);

        void Compile(TransientTexturePool &pool, rhi::FrameContext &frame);

        void Execute(rhi::CommandContext &commands, rhi::FrameContext &frame, std::span<const TextureBinding> imports,
                     const FrameInputs &inputs);

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
            uint32_t timerId = 0;
            ExecuteFn execute;
            std::vector<ColorTarget> colors;
            std::optional<DepthTarget> depth;
        };
        struct StateFix {
            ID3D12Resource *resource = nullptr;
            D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_COMMON;
        };

        void AcquireTransientTextures(TransientTexturePool &pool, rhi::FrameContext &frame);

        void AddUse(FgTexture texture, fg::Access access);

        void BindTargetsAndClear(const Pass &pass, rhi::CommandContext &commands) const;
        [[nodiscard]] const TextureBinding &BindingOf(FgTexture texture) const;
        [[nodiscard]] const TransientTextureDesc *TransientDescOf(FgTexture texture) const;

        ID3D12RootSignature *m_rootSignature;
        rhi::GpuProfiler &m_profiler;
        fg::GraphDesc m_desc;
        std::vector<std::string> m_textureNames;
        std::vector<TransientTextureDesc> m_transientDescs;
        std::vector<Pass> m_passes;
        std::vector<fg::TextureUse> m_setupUses;
        fg::CompiledGraph m_compiled;
        std::vector<TextureBinding> m_gpuBindings;
        std::vector<uint32_t> m_gpuShaderIndices;
        std::vector<StateFix> m_stateFixes;
        std::vector<TransientTexturePool::Request> m_poolRequests;
        std::vector<TransientTexturePool::Texture> m_poolTextures;
    };
} // namespace render

#endif // D3D12_ENGINE_FRAMEGRAPH_H
