#ifndef D3D12_ENGINE_TEXTURE_H
#define D3D12_ENGINE_TEXTURE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/Assert.h"
#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class BindlessHeap;
    class FrameContext;
    class UploadContext;

    // One mip level of a texture's pixels in CPU memory
    struct TextureMipData {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t rowPitch = 0;             // bytes per row
        std::span<const std::byte> pixels; // the whole level
    };

    // A 2D texture in GPU memory with all its mips. Shaders read it as ResourceDescriptorHeap[BindlessIndex()].
    //
    // Release it with Destroy(), not just the destructor: the GPU may still be using it, so the release has to
    // wait for a frame. The destructor checks that Destroy() was called. It can be moved but not assigned,
    // because assigning would throw the old texture away without Destroy().
    class Texture {
    public:
        Texture() = default;
        ~Texture();

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;
        Texture(Texture &&) noexcept = default;
        Texture &operator=(Texture &&) = delete;

        // Creates the texture, records the copy of `mips` into the upload batch and gives it a bindless slot.
        // mips[0] is the full size; each next level is half as large.
        [[nodiscard]] static Texture Create(Allocator &allocator, UploadContext &upload, BindlessHeap &heap,
                                            DXGI_FORMAT format, std::span<const TextureMipData> mips,
                                            std::wstring_view name);

        // Frees the texture and its slot once the GPU has finished `frame`
        void Destroy(FrameContext &frame, BindlessHeap &heap);

        [[nodiscard]] bool IsValid() const noexcept { return m_allocation != nullptr; }
        [[nodiscard]] uint32_t BindlessIndex() const {
            ENGINE_ASSERT(IsValid());
            return m_bindlessIndex;
        }
        [[nodiscard]] ID3D12Resource *Native() const noexcept {
            return m_allocation ? m_allocation->GetResource() : nullptr;
        }

    private:
        Allocation m_allocation;
        uint32_t m_bindlessIndex = 0;
    };
} // namespace rhi

#endif // D3D12_ENGINE_TEXTURE_H
