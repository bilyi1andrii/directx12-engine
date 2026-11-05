#ifndef D3D12_ENGINE_TEXTURELOADER_H
#define D3D12_ENGINE_TEXTURELOADER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

#include "core/Error.h"

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

    // Where one mip level lies in TextureData::pixels
    struct TextureMip {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t offset = 0;     // bytes from the start of pixels
        uint64_t rowPitch = 0;   // bytes per row; blocks for compressed formats
        uint64_t slicePitch = 0; // bytes in the whole level
    };

    // A decoded 2D texture with its mip chain, ready to be copied to the GPU. CPU only.
    struct TextureData {
        uint32_t dxgiFormat = 0;      // a DXGI_FORMAT value (this layer does not include DXGI headers)
        bool isSrgb = false;          // false if the format has no sRGB version, even for a colour texture
        std::vector<TextureMip> mips; // [0] is the full-size level
        std::vector<std::byte> pixels;
    };

    // Loads a .dds, .tga, .png, .jpg, ... file and generates its mips. Needs a core::ComScope on this thread.
    // Only plain 2D textures: arrays, cube maps and 3D textures return UnsupportedFormat.
    [[nodiscard]] std::expected<TextureData, core::Error> LoadTexture(const std::filesystem::path &path,
                                                                      TextureUsage usage);

    // Builds a texture from RGBA8 pixels made in code (exactly width * height * 4 bytes) and generates its mips
    [[nodiscard]] std::expected<TextureData, core::Error>
    CreateTextureFromPixels(std::span<const std::byte> rgba8, uint32_t width, uint32_t height, TextureUsage usage);
} // namespace engine::assets

#endif // D3D12_ENGINE_TEXTURELOADER_H
