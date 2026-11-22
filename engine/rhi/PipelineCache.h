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
    // Fixed-function PSO settings (blending, culling, depth)
    enum class BlendMode : uint8_t {
        Opaque,     // the pixel's colour replaces the target's
        AlphaBlend, // the pixel covers the target by its alpha: colour * alpha + target * (1 - alpha)
    };

    enum class CullMode : uint8_t {
        Back,  // draws the triangles that face the camera
        None,  // draws both sides
        Front, // draws only the triangles that face away: the far side of a closed shape
    };

    enum class DepthMode : uint8_t {
        TestAndWrite, // hides what is behind the depth already drawn, and stores its own depth
        TestOnly,     // hides what is behind, but leaves the depth as it is (transparent surfaces)
    };

    // Everything required to build a PSO. Uses views/spans to avoid copying,
    // so the underlying data must remain valid during the PipelineCache::Get() call.
    struct GraphicsPipelineDesc {
        std::wstring_view vertexShader; // compiled .cso file path/name
        std::wstring_view pixelShader;

        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout;
        BlendMode blend = BlendMode::Opaque;
        CullMode cull = CullMode::Back;
        DepthMode depth = DepthMode::TestAndWrite;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        std::span<const DXGI_FORMAT> renderTargetFormats;
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
        std::wstring_view name;
    };

    // A pipeline in the cache, as an index. Resolve it with PipelineCache::Native when recording.
    struct PipelineHandle {
        uint32_t index = std::numeric_limits<uint32_t>::max(); // default: invalid

        [[nodiscard]] bool IsValid() const noexcept { return index != std::numeric_limits<uint32_t>::max(); }
        bool operator==(const PipelineHandle &) const = default;
    };

    // Caches PSOs to avoid slow on-the-fly compilation.
    // Also owns the shared root signature and loaded shader bytecode.
    // Not thread-safe, non-copyable.
    class PipelineCache {
    public:
        PipelineCache(NativeDevice *device, std::filesystem::path shaderDirectory);

        PipelineCache(const PipelineCache &) = delete;
        PipelineCache &operator=(const PipelineCache &) = delete;
        PipelineCache(PipelineCache &&) = delete;
        PipelineCache &operator=(PipelineCache &&) = delete;

        // Returns an existing PSO or creates a new one if the description is unique
        [[nodiscard]] PipelineHandle Get(const GraphicsPipelineDesc &desc);

        [[nodiscard]] ID3D12PipelineState *Native(PipelineHandle handle) const;

        // Global root signature, shared across all PSOs in the engine
        [[nodiscard]] ID3D12RootSignature *RootSignature() const noexcept { return m_rootSignature.Get(); }

        [[nodiscard]] size_t PipelineCount() const noexcept { return m_pipelines.size(); }

    private:
        // Owns a copy of input layout element data to be stored as a unique map key
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

        // Full key with owned memory for fast PSO lookup in the unordered_map
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

        // Reads and caches shader bytecode from disk upon the first request
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
