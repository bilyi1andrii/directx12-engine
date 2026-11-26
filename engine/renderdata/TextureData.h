#ifndef D3D12_ENGINE_TEXTUREDATA_H
#define D3D12_ENGINE_TEXTUREDATA_H

// A decoded texture with all its mips, ready to upload to the GPU.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace render {
    struct TextureMip {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t offset = 0;     // bytes
        uint64_t rowPitch = 0;   // bytes
        uint64_t slicePitch = 0; // bytes
    };

    struct TextureData {
        uint32_t dxgiFormat = 0; // DXGI_FORMAT
        bool isSrgb = false;
        std::vector<TextureMip> mips;
        std::vector<std::byte> pixels;
    };
} // namespace render

#endif // D3D12_ENGINE_TEXTUREDATA_H
