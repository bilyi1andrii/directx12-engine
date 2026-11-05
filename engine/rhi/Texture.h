#ifndef D3D12_ENGINE_TEXTURE_H
#define D3D12_ENGINE_TEXTURE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/Assert.h"
#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class BindlessHeap;
    class FrameContext;
    class UploadContext;

    // One mip level of a texture's initial data, as the CPU holds it
    struct TextureMipData {
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t rowPitch = 0;             // bytes from one row to the next (a row of 4x4 blocks if compressed)
        std::span<const std::byte> pixels; // the whole level
    };

    // A 2D texture in the default heap (GPU-local memory) with its mip chain and a permanent bindless shader
    // resource view (SRV): shaders read it as ResourceDescriptorHeap[BindlessIndex()]. It stays in
    // PIXEL_SHADER_RESOURCE (textures, unlike buffers, keep their state between command lists).
    //
    // Created through rhi::Allocator; its data is copied by an UploadContext batch, so it is ready for command
    // lists submitted after that batch.
    //
    // Release it with Destroy(), which waits for a frame's fence like every GPU resource. The destructor asserts
    // that it was called (except while an exception unwinds). Move-constructible only: assigning would drop the
    // target without Destroy(). A default-constructed Texture is empty.
    class Texture {
    public:
        Texture() = default;
        ~Texture();

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;
        Texture(Texture &&) noexcept = default;
        Texture &operator=(Texture &&) = delete;

        // Creates a texture of `format` with mips.size() levels. mips[0] is the full size, every next level half
        // the previous one (rounded down, at least 1). Records the copy of all levels into `upload`'s batch (which
        // must be recording) and writes the SRV into a new slot of `heap`.
        [[nodiscard]] static Texture Create(Allocator &allocator, UploadContext &upload, BindlessHeap &heap,
                                            DXGI_FORMAT format, std::span<const TextureMipData> mips,
                                            std::wstring_view name);

        // Releases the texture and its bindless slot once the GPU has finished `frame`; this object becomes empty
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
} // namespace engine::rhi

#endif // D3D12_ENGINE_TEXTURE_H
