#include "rhi/Texture.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string>
#include <utility>

#include <directx/d3dx12.h>

#include "core/ThrowIfFailed.h"
#include "rhi/BindlessHeap.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadContext.h"
#include "rhi/UploadRing.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    Texture::~Texture() {
        // While an exception unwinds, a failed assert would hide it; the process is ending anyway
        ENGINE_ASSERT_MSG(!IsValid() || std::uncaught_exceptions() > 0,
                          "Texture with bindless index {} destroyed without Destroy()", m_bindlessIndex);
    }

    Texture Texture::Create(Allocator &allocator, UploadContext &upload, BindlessHeap &heap, DXGI_FORMAT format,
                            std::span<const TextureMipData> mips, std::wstring_view name) {
        ENGINE_ASSERT_MSG(!mips.empty() && mips.size() <= D3D12_REQ_MIP_LEVELS, "Texture with {} mip levels",
                          mips.size());
        const auto mipCount = static_cast<uint32_t>(mips.size());
        const D3D12_RESOURCE_DESC desc =
            CD3DX12_RESOURCE_DESC::Tex2D(format, mips[0].width, mips[0].height, 1, static_cast<UINT16>(mipCount));

        // Created in COPY_DEST, ready for the copy: unlike buffers, textures keep the initial state they are
        // created in, and they are not promoted from COMMON to everything a buffer is
        Texture texture;
        const std::wstring textureName(name);
        texture.m_allocation = allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, textureName);
        ID3D12Resource *resource = texture.Native();

        // The GPU stores textures in its own tiled layout, so it copies them from a buffer only in a fixed
        // linear layout: each row starts at a multiple of 256 bytes (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) and
        // each mip at a multiple of 512 (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT). GetCopyableFootprints
        // computes that layout for every mip: its offset and padded row pitch (the "footprint"), its number of
        // rows, the unpadded size of one row, and the total size.
        std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, D3D12_REQ_MIP_LEVELS> footprints{};
        std::array<UINT, D3D12_REQ_MIP_LEVELS> rowCounts{};
        std::array<UINT64, D3D12_REQ_MIP_LEVELS> rowSizes{};
        UINT64 totalBytes = 0;
        ComPtr<ID3D12Device> device;
        ThrowIfFailed(resource->GetDevice(IID_PPV_ARGS(&device)));
        device->GetCopyableFootprints(&desc, 0, mipCount, 0, footprints.data(), rowCounts.data(), rowSizes.data(),
                                      &totalBytes);

        // Its own staging buffer starts at offset 0, so the footprints' 512-byte placement holds in it
        const UploadAllocation staging = upload.AllocateStaging(totalBytes, textureName + L" upload");
        ENGINE_ASSERT(staging.offset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0);

        CommandContext &commands = upload.Commands();
        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            const TextureMipData &source = mips[mip];
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = footprints[mip];
            const uint64_t rowSize = rowSizes[mip];
            const uint32_t rowCount = rowCounts[mip];

            // ASan cannot see writes into upload-heap memory, so check every bound here. WriteRange checks the
            // destination against the staging buffer's size.
            ENGINE_ASSERT_MSG(source.width == std::max(1u, mips[0].width >> mip) &&
                                  source.height == std::max(1u, mips[0].height >> mip),
                              "Mip {} is {}x{}, expected half of the previous level", mip, source.width, source.height);
            ENGINE_ASSERT_MSG(rowSize <= source.rowPitch && rowSize <= footprint.Footprint.RowPitch,
                              "Mip {}: row of {} bytes, source pitch {}, footprint pitch {}", mip, rowSize,
                              source.rowPitch, footprint.Footprint.RowPitch);
            ENGINE_ASSERT_MSG((rowCount - 1) * source.rowPitch + rowSize <= source.pixels.size(),
                              "Mip {}: {} rows of {} bytes do not fit in {} bytes of pixels", mip, rowCount, rowSize,
                              source.pixels.size());

            // Row by row: the source rows are tightly packed, the footprint's are padded to its row pitch
            for (uint32_t row = 0; row < rowCount; ++row) {
                staging.WriteRange(footprint.Offset + uint64_t{row} * footprint.Footprint.RowPitch,
                                   source.pixels.subspan(row * source.rowPitch, rowSize));
            }

            // A copy location names either a subresource (here: mip `mip`) of a texture, or a footprint in a buffer
            footprint.Offset += staging.offset;
            commands.CopyTextureRegion(CD3DX12_TEXTURE_COPY_LOCATION(resource, mip),
                                       CD3DX12_TEXTURE_COPY_LOCATION(staging.resource, footprint));
        }
        commands.Transition(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // Last, so nothing above can throw with the slot taken
        texture.m_bindlessIndex = heap.Allocate();
        heap.CreateTexture2DSrv(texture.m_bindlessIndex, resource, format, mipCount);
        return texture;
    }

    void Texture::Destroy(FrameContext &frame, BindlessHeap &heap) {
        ENGINE_ASSERT_MSG(IsValid(), "Texture::Destroy on an empty texture");
        // Shaders of frames still in flight may read the slot, and their commands the resource
        frame.DeferFree(heap, m_bindlessIndex);
        frame.DeferRelease(std::move(m_allocation));
        m_allocation = nullptr; // a moved-from ComPtr is already null; this makes it explicit
        m_bindlessIndex = 0;
    }
} // namespace engine::rhi
