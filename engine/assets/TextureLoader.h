#ifndef D3D12_ENGINE_TEXTURELOADER_H
#define D3D12_ENGINE_TEXTURELOADER_H

// Decodes image files and in-memory images into textures with all their mips, read as colour (sRGB) or as plain
// data, depending on what the texture holds.

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>

#include "core/Error.h"
#include "renderdata/TextureData.h"

namespace assets {
    enum class TextureUsage {
        Unknown,
        Diffuse,
        BaseColor,
        Emissive,
        Specular,
        Normals,
        Height,
        Opacity,
        Metalness,
        Roughness,
        AmbientOcclusion,
    };

    [[nodiscard]] bool IsSrgbUsage(TextureUsage usage) noexcept;

    [[nodiscard]] std::expected<render::TextureData, core::Error> ImportTexture(const std::filesystem::path &path,
                                                                                TextureUsage usage);

    [[nodiscard]] std::expected<render::TextureData, core::Error>
    ImportTextureFromMemory(std::span<const std::byte> bytes, std::string_view formatHint, TextureUsage usage);

    [[nodiscard]] std::expected<render::TextureData, core::Error>
    MakeTextureFromPixels(std::span<const std::byte> rgba8, uint32_t width, uint32_t height, TextureUsage usage);
} // namespace assets

#endif // D3D12_ENGINE_TEXTURELOADER_H
