#ifndef D3D12_ENGINE_MATERIALDESC_H
#define D3D12_ENGINE_MATERIALDESC_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

#include <DirectXMath.h>

namespace engine::render {
    enum class AlphaMode : uint8_t {
        Opaque, // alpha is ignored
        Mask,   // pixels with alpha below alphaCutoff are discarded
        Blend,  // transparent, drawn back to front
    };

    // A texture stored inside the model file: index into ModelData::embeddedTextures
    struct EmbeddedTexture {
        uint32_t index = 0;

        bool operator==(const EmbeddedTexture &) const = default;
    };

    // Where a texture comes from: nowhere, a file (absolute, normalized path), or an image inside the model file
    using TextureSource = std::variant<std::monostate, std::filesystem::path, EmbeddedTexture>;

    // A material read from a model file: colours, factors and textures. Until PBR exists, only albedo is used.
    struct MaterialDesc {
        std::string name;

        DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f}; // multiplies the albedo texture; w is the opacity
        float metallicFactor = 1.0f;                         // multiplies the metallic texture
        float roughnessFactor = 1.0f;                        // multiplies the roughness texture
        DirectX::XMFLOAT3 emissiveFactor{0.0f, 0.0f, 0.0f};

        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f; // for AlphaMode::Mask
        bool doubleSided = false; // draw back faces too (leaves, curtains)

        TextureSource albedo;
        TextureSource normal;
        TextureSource emissive;
        TextureSource height;
        TextureSource opacityMap;
        // Either two separate textures, or one packed texture (glTF: roughness in green, metallic in blue).
        // When packed, both point to the same texture.
        TextureSource metallic;
        TextureSource roughness;
        bool packedMetallicRoughness = false;
        TextureSource ambientOcclusion; // read from the red channel
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MATERIALDESC_H
