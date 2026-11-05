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
        uint32_t dxgiFormat = 0;      // value of DXGI_FORMAT: this layer does not include DXGI headers
        bool isSrgb = false;          // false for a colour texture whose format has no sRGB variant (e.g. R8_UNORM)
        std::vector<TextureMip> mips; // [0] is the full-size level
        std::vector<std::byte> pixels;
    };

    // Loads a texture by file extension: .dds, .tga, anything else through WIC (.png, .jpg, ...).
    // WIC needs a core::ComScope on this thread. Generates mips on the CPU unless the file has its own or is
    // compressed. Array, cube and volume textures return UnsupportedFormat.
    [[nodiscard]] std::expected<TextureData, core::Error> LoadTexture(const std::filesystem::path &path,
                                                                      TextureUsage usage);

    // Builds a texture from `width` x `height` tightly packed RGBA8 pixels (e.g. made in code) and generates its
    // mips. `rgba8` must hold exactly width * height * 4 bytes. Needs no COM.
    [[nodiscard]] std::expected<TextureData, core::Error>
    CreateTextureFromPixels(std::span<const std::byte> rgba8, uint32_t width, uint32_t height, TextureUsage usage);
} // namespace engine::assets

#endif // D3D12_ENGINE_TEXTURELOADER_H
