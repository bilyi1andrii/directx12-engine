#ifndef D3D12_ENGINE_MATERIALDESC_H
#define D3D12_ENGINE_MATERIALDESC_H

// A material as read from a model file: its colours, factors, and the textures it names, which may be files or
// images stored inside the model.

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

#include <DirectXMath.h>

namespace render {
    enum class AlphaMode : uint8_t {
        Opaque,
        Mask,
        Blend,
    };

    struct EmbeddedTexture {
        uint32_t index = 0;

        bool operator==(const EmbeddedTexture &) const = default;
    };

    // none, file path or image embedded in model
    using TextureSource = std::variant<std::monostate, std::filesystem::path, EmbeddedTexture>;

    struct MaterialDesc {
        std::string name;

        DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        DirectX::XMFLOAT3 emissiveFactor{0.0f, 0.0f, 0.0f};

        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;

        TextureSource albedo;
        TextureSource normal;
        TextureSource emissive;
        TextureSource height;
        TextureSource opacityMap;
        TextureSource metallic;
        TextureSource roughness;
        bool packedMetallicRoughness = false;
        TextureSource ambientOcclusion;
    };
} // namespace render

#endif // D3D12_ENGINE_MATERIALDESC_H
