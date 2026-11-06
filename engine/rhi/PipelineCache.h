#ifndef D3D12_ENGINE_PIPELINECACHE_H
#define D3D12_ENGINE_PIPELINECACHE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // Fixed-function settings of a PSO. Each enum lists only what the engine draws; values are added
    // with the features that need them (transparency, shadows, double-sided materials).
    enum class BlendMode : uint8_t {
        Opaque, // the pixel shader's colour replaces the target
    };

    enum class CullMode : uint8_t {
        Back, // back faces (counter-clockwise on screen) are not drawn
        None, // both sides are drawn
    };

    enum class DepthMode : uint8_t {
        TestAndWrite, // draw only what is nearer than the depth buffer (LESS), then store the new depth
    };

    // Everything a graphics PSO is built from. Plain data: two equal descriptions give the same PSO.
    // The views and spans only need to stay valid during PipelineCache::Get().
    struct GraphicsPipelineDesc {
        std::wstring_view vertexShader; // compiled .cso file name in the cache's shader directory
        std::wstring_view pixelShader;
        // How the input assembler reads one vertex. Compared by content, so two identical layouts share a PSO.
        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout;
        BlendMode blend = BlendMode::Opaque;
        CullMode cull = CullMode::Back;
        DepthMode depth = DepthMode::TestAndWrite;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        // Formats of the render targets the PSO draws into, in slot order (1 to 8). They must match the
        // bound views, so take them from SwapChain::kRtvFormat.
        std::span<const DXGI_FORMAT> renderTargetFormats;
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN; // DepthBuffer::kFormat; UNKNOWN: no depth buffer
        std::wstring_view name;                        // debug name for the debug layer and PIX; not part of the key
    };

    // A PSO owned by the cache. Stays valid as long as the cache lives; resolve it with PipelineCache::Native()
    // when recording. A handle instead of a pointer lets the cache replace the PSO behind it later (shader hot
    // reload) without changing the code that holds it.
    struct PipelineHandle {
        uint32_t index = std::numeric_limits<uint32_t>::max(); // default: no pipeline

        [[nodiscard]] bool IsValid() const noexcept { return index != std::numeric_limits<uint32_t>::max(); }
        bool operator==(const PipelineHandle &) const = default;
    };

    // Creates graphics pipeline state objects (PSOs) from a GraphicsPipelineDesc and keeps them, so the same
    // description never builds a PSO twice. Also owns the global root signature that every PSO
    // uses, and the bytecode of every shader it has loaded.
    //
    // A PSO bakes shaders and all fixed-function state (blend, rasterizer, depth, input layout, target
    // formats) into one object up front, so the driver compiles it once instead of at draw time. Creating one
    // takes milliseconds, which is why they are cached; switching between existing ones is cheap.
    //
    // Not thread-safe. Not copyable or movable (handles index into it).
    class PipelineCache {
    public:
        // Loads the global root signature (rs_global.cso) from `shaderDirectory`. Throws if it cannot.
        PipelineCache(NativeDevice *device, std::filesystem::path shaderDirectory);

        PipelineCache(const PipelineCache &) = delete;
        PipelineCache &operator=(const PipelineCache &) = delete;
        PipelineCache(PipelineCache &&) = delete;
        PipelineCache &operator=(PipelineCache &&) = delete;

        // The PSO for `desc`: an existing one, or a new one created now. Throws if a shader file is missing or
        // the device rejects the description (the debug layer logs why).
        [[nodiscard]] PipelineHandle Get(const GraphicsPipelineDesc &desc);

        [[nodiscard]] ID3D12PipelineState *Native(PipelineHandle handle) const;

        // The engine's one root signature (shaders/common/root_signature.hlsli), shared by every PSO
        [[nodiscard]] ID3D12RootSignature *RootSignature() const noexcept { return m_rootSignature.Get(); }

        [[nodiscard]] size_t PipelineCount() const noexcept { return m_pipelines.size(); }

    private:
        // A GraphicsPipelineDesc with copies of everything it points to, so it can be stored as a map key.
        // Two keys are equal when every field is, including each input element's semantic name.
        struct InputElement {
            std::string semanticName;
            uint32_t semanticIndex = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            uint32_t inputSlot = 0;
            uint32_t alignedByteOffset = 0;
            D3D12_INPUT_CLASSIFICATION inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            uint32_t instanceDataStepRate = 0;

            bool operator==(const InputElement &) const = default;
        };

        struct Key {
            std::wstring vertexShader;
            std::wstring pixelShader;
            std::vector<InputElement> inputLayout;
            BlendMode blend = BlendMode::Opaque;
            CullMode cull = CullMode::Back;
            DepthMode depth = DepthMode::TestAndWrite;
            D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            std::vector<DXGI_FORMAT> renderTargetFormats;
            DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;

            bool operator==(const Key &) const = default;
        };

        struct KeyHash {
            [[nodiscard]] size_t operator()(const Key &key) const noexcept;
        };

        [[nodiscard]] static Key MakeKey(const GraphicsPipelineDesc &desc);

        [[nodiscard]] ComPtr<ID3D12PipelineState> Create(const GraphicsPipelineDesc &desc);

        // The compiled bytecode of `fileName`, read from disk the first time it is asked for
        [[nodiscard]] const std::vector<std::byte> &Shader(std::wstring_view fileName);

        NativeDevice *m_device;
        std::filesystem::path m_shaderDirectory;
        ComPtr<ID3D12RootSignature> m_rootSignature;
        std::vector<ComPtr<ID3D12PipelineState>> m_pipelines; // PipelineHandle::index points here
        std::unordered_map<Key, uint32_t, KeyHash> m_indexByKey;
        std::unordered_map<std::wstring, std::vector<std::byte>> m_shaders; // by file name
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_PIPELINECACHE_H
