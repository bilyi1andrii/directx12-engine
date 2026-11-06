#ifndef D3D12_ENGINE_TEXTURELOADER_H
#define D3D12_ENGINE_TEXTURELOADER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>

#include "core/Error.h"
#include "renderdata/TextureData.h"

namespace engine::assets {
    // What a texture holds, which decides how its bytes are read. 
    enum class TextureUsage {
        Unknown,
        // Colour: sRGB
        Diffuse,
        BaseColor,
        Emissive,
        Specular,
        // Data: plain numbers, UNORM
        Normals,
        Height,
        Opacity,
        Metalness,
        Roughness,
        AmbientOcclusion,
    };

    [[nodiscard]] bool IsSrgbUsage(TextureUsage usage) noexcept;

    // Loads a .dds, .tga, .png, .jpg, ... file and generates its mips. Needs a core::ComScope on this thread.
    // Only plain 2D textures: arrays, cube maps and 3D textures return UnsupportedFormat.
    [[nodiscard]] std::expected<render::TextureData, core::Error> LoadTexture(const std::filesystem::path &path,
                                                                              TextureUsage usage);

    // Same as LoadTexture, from the bytes of an image file held in memory: a texture stored inside a model file.
    // `formatHint` is the file type without the dot, any case ("png", "jpg", "dds", "tga"; render::EmbeddedTextureData
    // keeps Assimp's). It picks the decoder like the extension does in LoadTexture; for anything but dds and tga
    // WIC reads the type from the bytes.
    [[nodiscard]] std::expected<render::TextureData, core::Error>
    LoadTextureFromMemory(std::span<const std::byte> bytes, std::string_view formatHint, TextureUsage usage);

    // Builds a texture from RGBA8 pixels made in code (exactly width * height * 4 bytes) and generates its mips
    [[nodiscard]] std::expected<render::TextureData, core::Error>
    CreateTextureFromPixels(std::span<const std::byte> rgba8, uint32_t width, uint32_t height, TextureUsage usage);
} // namespace engine::assets

#endif // D3D12_ENGINE_TEXTURELOADER_H
