#ifndef D3D12_ENGINE_MATERIALDESC_H
#define D3D12_ENGINE_MATERIALDESC_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

#include <DirectXMath.h>

namespace engine::render {
    // How a material's alpha is used
    enum class AlphaMode : uint8_t {
        Opaque, // alpha ignored
        Mask,   // alpha test against MaterialDesc::alphaCutoff (step 4)
        Blend,  // transparent: drawn in the transparent pass, sorted back to front (step 4)
    };

    // A texture stored inside the model file: index into ModelData::embeddedTextures
    struct EmbeddedTexture {
        uint32_t index = 0;

        bool operator==(const EmbeddedTexture &) const = default;
    };

    // Where a material's texture comes from: nowhere (monostate), a file, or an image embedded in the model file.
    // File paths are absolute and normalized (std::filesystem::weakly_canonical), so the same file reached
    // through different relative paths is loaded once.
    using TextureSource = std::variant<std::monostate, std::filesystem::path, EmbeddedTexture>;

    // A material as read from a model file: factors and where its textures are. CPU only; the GPU material
    // buffer comes in step 6. Until then only `albedo` is used.
    struct MaterialDesc {
        std::string name;

        // Multiplies the albedo texture (glTF baseColorFactor; DX11: the diffuse colour, made into a 1x1 texture
        // when there is no texture file). .w is the material's opacity (also AI_MATKEY_OPACITY, DX11
        // Mesh::opacity): the only opacity value.
        DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallicFactor = 1.0f;  // multiplies the metallic value (glTF metallicFactor)
        float roughnessFactor = 1.0f; // multiplies the roughness value (glTF roughnessFactor)
        DirectX::XMFLOAT3 emissiveFactor{0.0f, 0.0f, 0.0f};

        // From the file when it states one (glTF); otherwise Blend when baseColor.w < 1 (the DX11 rule)
        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f; // for AlphaMode::Mask
        bool doubleSided = false; // draw back faces too (leaves, curtains)

        TextureSource albedo;
        TextureSource normal;
        TextureSource emissive;
        TextureSource height;
        TextureSource opacityMap;
        // Metallic and roughness: either two separate files (DX11 suffix maps, value in red) or one packed file
        // (glTF: green = roughness, blue = metallic); in the packed case both point to the same texture.
        TextureSource metallic;
        TextureSource roughness;
        bool packedMetallicRoughness = false;
        TextureSource ambientOcclusion; // red channel; in glTF often the same file as metallic/roughness
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MATERIALDESC_H
