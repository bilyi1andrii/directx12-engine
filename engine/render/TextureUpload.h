#ifndef D3D12_ENGINE_TEXTUREUPLOAD_H
#define D3D12_ENGINE_TEXTUREUPLOAD_H

#include <cstddef>
#include <cstdint>
#include <span>

namespace engine::render {
    // A texture's initial data as Renderer::CreateTexture takes it. Plain CPU data without D3D12 types, so code
    // above the renderer can fill it; the sandbox fills it from assets::TextureData (render cannot include assets).
    // Where this lives is revisited with the RenderList location question (docs/PROGRESS.md) before model loading.

    // One mip level
    struct TextureMipUpload {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t rowPitch = 0;             // bytes from one row to the next (a row of 4x4 blocks if compressed)
        std::span<const std::byte> pixels; // the whole level
    };

    struct TextureUpload {
        uint32_t dxgiFormat = 0;                // value of DXGI_FORMAT
        std::span<const TextureMipUpload> mips; // [0] is the full-size level, each next one half as large
    };
} // namespace engine::render

#endif // D3D12_ENGINE_TEXTUREUPLOAD_H
