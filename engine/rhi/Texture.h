#ifndef D3D12_ENGINE_TEXTURE_H
#define D3D12_ENGINE_TEXTURE_H

// A 2D texture with all its mips in GPU memory, uploaded through a batch and read by shaders through its bindless
// slot. It is freed with a frame, after the GPU has finished with it.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class BindlessHeap;
    class FrameContext;
    class UploadContext;

    struct TextureMipData {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t rowPitch = 0; // bytes
        std::span<const std::byte> pixels;
    };

    class Texture {
    public:
        Texture() = default;
        ~Texture();

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;
        Texture(Texture &&) noexcept = default;
        Texture &operator=(Texture &&) = delete;

        [[nodiscard]] static Texture Create(Allocator &allocator, UploadContext &upload, BindlessHeap &heap,
                                            DXGI_FORMAT format, std::span<const TextureMipData> mips,
                                            std::wstring_view name);

        void Destroy(FrameContext &frame, BindlessHeap &heap);

        [[nodiscard]] bool IsValid() const noexcept { return m_allocation != nullptr; }
        [[nodiscard]] uint32_t BindlessIndex() const noexcept { return m_bindlessIndex; }
        [[nodiscard]] ID3D12Resource *Native() const noexcept { return m_allocation->GetResource(); }

    private:
        Allocation m_allocation;
        uint32_t m_bindlessIndex = 0;
    };
} // namespace rhi

#endif // D3D12_ENGINE_TEXTURE_H
