#ifndef D3D12_ENGINE_PIPELINECACHE_H
#define D3D12_ENGINE_PIPELINECACHE_H

// Builds each pipeline state once from its description and returns it from the cache afterwards. It also loads
// the shaders and the root signature that every pipeline shares.

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

namespace rhi {
    enum class BlendMode : uint8_t {
        Opaque,
        AlphaBlend,
    };

    enum class CullMode : uint8_t {
        Back,
        None,
        Front,
    };

    enum class DepthMode : uint8_t {
        TestAndWrite,
        TestOnly,
    };

    struct DepthBias {
        int32_t constant = 0;
        float slopeScaled = 0.0f;

        bool operator==(const DepthBias &) const = default;
    };

    struct GraphicsPipelineDesc {
        std::wstring_view vertexShader; // .cso file name
        std::wstring_view pixelShader;

        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout;
        BlendMode blend = BlendMode::Opaque;
        CullMode cull = CullMode::Back;
        DepthMode depth = DepthMode::TestAndWrite;
        DepthBias depthBias;

        std::span<const DXGI_FORMAT> renderTargetFormats;
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
        std::wstring_view name;
    };

    // index into cache, resolved by Native() when recording
    struct PipelineHandle {
        uint32_t index = std::numeric_limits<uint32_t>::max();

        [[nodiscard]] bool IsValid() const noexcept { return index != std::numeric_limits<uint32_t>::max(); }
        bool operator==(const PipelineHandle &) const = default;
    };

    class PipelineCache {
    public:
        PipelineCache(NativeDevice *device, std::filesystem::path shaderDirectory);

        PipelineCache(const PipelineCache &) = delete;
        PipelineCache &operator=(const PipelineCache &) = delete;
        PipelineCache(PipelineCache &&) = delete;
        PipelineCache &operator=(PipelineCache &&) = delete;

        [[nodiscard]] PipelineHandle Get(const GraphicsPipelineDesc &desc);

        [[nodiscard]] ID3D12PipelineState *Native(PipelineHandle handle) const;

        [[nodiscard]] ID3D12RootSignature *RootSignature() const noexcept { return m_rootSignature.Get(); }

        [[nodiscard]] size_t PipelineCount() const noexcept { return m_pipelines.size(); }

    private:
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

        // copies desc's strings and arrays, so key never points at caller's memory
        struct Key {
            std::wstring vertexShader;
            std::wstring pixelShader;
            std::vector<InputElement> inputLayout;
            BlendMode blend = BlendMode::Opaque;
            CullMode cull = CullMode::Back;
            DepthMode depth = DepthMode::TestAndWrite;
            DepthBias depthBias;
            std::vector<DXGI_FORMAT> renderTargetFormats;
            DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;

            bool operator==(const Key &) const = default;
        };

        struct KeyHash {
            [[nodiscard]] size_t operator()(const Key &key) const noexcept;
        };

        [[nodiscard]] static Key MakeKey(const GraphicsPipelineDesc &desc);

        [[nodiscard]] ComPtr<ID3D12PipelineState> Create(const GraphicsPipelineDesc &desc);

        [[nodiscard]] const std::vector<std::byte> &Shader(std::wstring_view fileName);

        NativeDevice *m_device;
        std::filesystem::path m_shaderDirectory;
        ComPtr<ID3D12RootSignature> m_rootSignature;
        std::vector<ComPtr<ID3D12PipelineState>> m_pipelines;
        std::unordered_map<Key, uint32_t, KeyHash> m_indexByKey;
        std::unordered_map<std::wstring, std::vector<std::byte>> m_shaders;
    };
} // namespace rhi

#endif // D3D12_ENGINE_PIPELINECACHE_H
