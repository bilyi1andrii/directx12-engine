#include "rhi/PipelineCache.h"

#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Utf8.h"
#include "rhi/DepthFormat.h"

namespace fs = std::filesystem;

namespace rhi {
    using core::ThrowIfFailed;

    namespace {
        // FNV-1a 64-bit hash
        class Hasher {
        public:
            void AddBytes(std::span<const std::byte> bytes) noexcept {
                for (const std::byte b : bytes) {
                    m_hash ^= static_cast<uint64_t>(b);
                    m_hash *= kPrime;
                }
            }

            template <typename T>
                requires std::is_trivially_copyable_v<T>
            void Add(const T &value) noexcept {
                AddBytes(std::as_bytes(std::span(&value, 1)));
            }

            template <typename Char> void AddString(std::basic_string_view<Char> text) noexcept {
                Add(text.size());
                AddBytes(std::as_bytes(std::span(text)));
            }

            [[nodiscard]] uint64_t Value() const noexcept { return m_hash; }

        private:
            static constexpr uint64_t kPrime = 1099511628211ull; // FNV prime
            uint64_t m_hash = 14695981039346656037ull;           // FNV offset basis
        };

        [[nodiscard]] D3D12_CULL_MODE NativeCullMode(CullMode mode) noexcept {
            switch (mode) {
            case CullMode::None:
                return D3D12_CULL_MODE_NONE;
            case CullMode::Front:
                return D3D12_CULL_MODE_FRONT;
            case CullMode::Back:
                break;
            }
            return D3D12_CULL_MODE_BACK;
        }

        [[nodiscard]] D3D12_DEPTH_WRITE_MASK NativeDepthWrite(DepthMode mode) noexcept {
            switch (mode) {
            case DepthMode::TestOnly:
                return D3D12_DEPTH_WRITE_MASK_ZERO;
            case DepthMode::TestAndWrite:
                break;
            }
            return D3D12_DEPTH_WRITE_MASK_ALL;
        }

        [[nodiscard]] D3D12_BLEND_DESC NativeBlend(BlendMode mode) noexcept {
            D3D12_BLEND_DESC blend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            if (mode == BlendMode::AlphaBlend) {
                D3D12_RENDER_TARGET_BLEND_DESC &target = blend.RenderTarget[0];
                target.BlendEnable = TRUE;
                target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                target.BlendOp = D3D12_BLEND_OP_ADD;
                target.SrcBlendAlpha = D3D12_BLEND_ONE;
                target.DestBlendAlpha = D3D12_BLEND_ZERO;
                target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            }
            return blend;
        }
    } // namespace

    PipelineCache::PipelineCache(NativeDevice *device, fs::path shaderDirectory)
        : m_device(device), m_shaderDirectory(std::move(shaderDirectory)) {
        const std::vector<std::byte> &rootSignature = Shader(L"rs_global.cso");
        ThrowIfFailed(m_device->CreateRootSignature(0, rootSignature.data(), rootSignature.size(),
                                                    IID_PPV_ARGS(&m_rootSignature)));
        ThrowIfFailed(m_rootSignature->SetName(L"Global root signature"));
    }

    PipelineHandle PipelineCache::Get(const GraphicsPipelineDesc &desc) {
        Key key = MakeKey(desc);
        if (const auto found = m_indexByKey.find(key); found != m_indexByKey.end()) {
            return PipelineHandle{.index = found->second};
        }

        ComPtr<ID3D12PipelineState> pipelineState = Create(desc);
        const auto index = static_cast<uint32_t>(m_pipelines.size());
        m_pipelines.push_back(std::move(pipelineState));
        m_indexByKey.emplace(std::move(key), index);
        core::LogInfo("PipelineCache: created \"{}\" ({} PSO(s) cached)", core::ToUtf8(desc.name), m_pipelines.size());
        return PipelineHandle{.index = index};
    }

    ID3D12PipelineState *PipelineCache::Native(PipelineHandle handle) const {
        return m_pipelines[handle.index].Get();
    }

    size_t PipelineCache::KeyHash::operator()(const Key &key) const noexcept {
        Hasher hasher;
        hasher.AddString(std::wstring_view(key.vertexShader));
        hasher.AddString(std::wstring_view(key.pixelShader));
        hasher.Add(key.inputLayout.size());
        for (const InputElement &element : key.inputLayout) {
            hasher.AddString(std::string_view(element.semanticName));
            hasher.Add(element.semanticIndex);
            hasher.Add(element.format);
            hasher.Add(element.inputSlot);
            hasher.Add(element.alignedByteOffset);
            hasher.Add(element.inputSlotClass);
            hasher.Add(element.instanceDataStepRate);
        }
        hasher.Add(key.blend);
        hasher.Add(key.cull);
        hasher.Add(key.depth);
        hasher.Add(key.depthBias.constant);
        hasher.Add(key.depthBias.slopeScaled);
        hasher.Add(key.renderTargetFormats.size());
        for (const DXGI_FORMAT format : key.renderTargetFormats) {
            hasher.Add(format);
        }
        hasher.Add(key.depthFormat);
        return static_cast<size_t>(hasher.Value());
    }

    PipelineCache::Key PipelineCache::MakeKey(const GraphicsPipelineDesc &desc) {
        ENGINE_ASSERT_MSG(desc.renderTargetFormats.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
                          "PSO \"{}\" has {} render targets (at most 8)", core::ToUtf8(desc.name),
                          desc.renderTargetFormats.size());

        Key key{
            .vertexShader = std::wstring(desc.vertexShader),
            .pixelShader = std::wstring(desc.pixelShader),
            .inputLayout = {},
            .blend = desc.blend,
            .cull = desc.cull,
            .depth = desc.depth,
            .depthBias = desc.depthBias,
            .renderTargetFormats = {desc.renderTargetFormats.begin(), desc.renderTargetFormats.end()},
            .depthFormat = desc.depthFormat,
        };
        key.inputLayout.reserve(desc.inputLayout.size());
        for (const D3D12_INPUT_ELEMENT_DESC &element : desc.inputLayout) {
            key.inputLayout.push_back({
                .semanticName = element.SemanticName,
                .semanticIndex = element.SemanticIndex,
                .format = element.Format,
                .inputSlot = element.InputSlot,
                .alignedByteOffset = element.AlignedByteOffset,
                .inputSlotClass = element.InputSlotClass,
                .instanceDataStepRate = element.InstanceDataStepRate,
            });
        }
        return key;
    }

    ComPtr<ID3D12PipelineState> PipelineCache::Create(const GraphicsPipelineDesc &desc) {
        const std::vector<std::byte> &vertexShader = Shader(desc.vertexShader);

        // no pixel shader gives a depth-only PSO
        const std::vector<std::byte> *pixelShader = desc.pixelShader.empty() ? nullptr : &Shader(desc.pixelShader);

        D3D12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rasterizer.CullMode = NativeCullMode(desc.cull);
        rasterizer.DepthBias = desc.depthBias.constant;
        rasterizer.SlopeScaledDepthBias = desc.depthBias.slopeScaled;

        D3D12_DEPTH_STENCIL_DESC depthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        depthStencil.DepthFunc = kDepthComparison; // reversed-Z
        depthStencil.DepthWriteMask = NativeDepthWrite(desc.depth);
        if (desc.depthFormat == DXGI_FORMAT_UNKNOWN) {
            depthStencil.DepthEnable = FALSE;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{
            .pRootSignature = m_rootSignature.Get(),
            .VS = {.pShaderBytecode = vertexShader.data(), .BytecodeLength = vertexShader.size()},
            .PS = pixelShader != nullptr ? D3D12_SHADER_BYTECODE{.pShaderBytecode = pixelShader->data(),
                                                                 .BytecodeLength = pixelShader->size()}
                                         : D3D12_SHADER_BYTECODE{},
            .BlendState = NativeBlend(desc.blend),
            .SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
            .RasterizerState = rasterizer,
            .DepthStencilState = depthStencil,
            .InputLayout = {.pInputElementDescs = desc.inputLayout.data(),
                            .NumElements = static_cast<UINT>(desc.inputLayout.size())},
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = static_cast<UINT>(desc.renderTargetFormats.size()),
            .DSVFormat = desc.depthFormat,
            .SampleDesc = {.Count = 1, .Quality = 0},
        };
        for (size_t i = 0; i < desc.renderTargetFormats.size(); ++i) {
            pipelineDesc.RTVFormats[i] = desc.renderTargetFormats[i];
        }

        ComPtr<ID3D12PipelineState> pipelineState;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState)));
        ThrowIfFailed(pipelineState->SetName(std::wstring(desc.name).c_str()));
        return pipelineState;
    }

    const std::vector<std::byte> &PipelineCache::Shader(std::wstring_view fileName) {
        const std::wstring key(fileName);
        if (const auto found = m_shaders.find(key); found != m_shaders.end()) {
            return found->second;
        }

        auto bytecode = core::ReadBinaryFile(m_shaderDirectory / fileName);
        if (!bytecode) {
            throw std::runtime_error(std::format("Cannot load shader {}", bytecode.error().message));
        }
        return m_shaders.emplace(key, std::move(*bytecode)).first->second;
    }
} // namespace rhi
