#ifndef D3D12_ENGINE_TEXTUREUPLOAD_H
#define D3D12_ENGINE_TEXTUREUPLOAD_H

#include <cstddef>
#include <cstdint>
#include <span>

namespace engine::render {
    // Pixels for Renderer::CreateTexture. Plain CPU data with no D3D12 types, so code above the renderer
    // can fill it (the sandbox fills it from assets::TextureData).

    // One mip level
    struct TextureMipUpload {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t rowPitch = 0;             // bytes per row
        std::span<const std::byte> pixels; // the whole level
    };

    struct TextureUpload {
        uint32_t dxgiFormat = 0;                // a DXGI_FORMAT value
        std::span<const TextureMipUpload> mips; // [0] is the full size, each next level half as large
    };
} // namespace engine::render

#endif // D3D12_ENGINE_TEXTUREUPLOAD_H
