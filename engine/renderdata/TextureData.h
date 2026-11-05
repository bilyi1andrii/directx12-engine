#ifndef D3D12_ENGINE_TEXTUREDATA_H
#define D3D12_ENGINE_TEXTUREDATA_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::render {
    // Where one mip level lies in TextureData::pixels
    struct TextureMip {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t offset = 0;     // bytes from the start of pixels
        uint64_t rowPitch = 0;   // bytes per row; blocks for compressed formats
        uint64_t slicePitch = 0; // bytes in the whole level
    };

    // A decoded 2D texture with its mips, ready to upload: TextureLoader makes it, Renderer::CreateTexture uploads it
    struct TextureData {
        uint32_t dxgiFormat = 0;      // a DXGI_FORMAT value (this layer does not include DXGI headers)
        bool isSrgb = false;          // false if the format has no sRGB version, even for a colour texture
        std::vector<TextureMip> mips; // [0] is the full-size level
        std::vector<std::byte> pixels;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_TEXTUREDATA_H
