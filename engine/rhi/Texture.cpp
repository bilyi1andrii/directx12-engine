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
        // Skipped while an exception unwinds: the assert would hide the real error
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

        // 1. The texture in GPU memory, created in COPY_DEST, ready to be copied into
        Texture texture;
        const std::wstring textureName(name);
        texture.m_allocation = allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, textureName);
        ID3D12Resource *resource = texture.Native();

        // 2. Ask where each mip must sit in the staging buffer (its "footprint"). The GPU copies textures from
        //    buffers only if every row starts at a multiple of 256 bytes, so rows get padded.
        std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, D3D12_REQ_MIP_LEVELS> footprints{};
        std::array<UINT, D3D12_REQ_MIP_LEVELS> rowCounts{};
        std::array<UINT64, D3D12_REQ_MIP_LEVELS> rowSizes{};
        UINT64 totalBytes = 0;
        ComPtr<ID3D12Device> device;
        ThrowIfFailed(resource->GetDevice(IID_PPV_ARGS(&device)));
        device->GetCopyableFootprints(&desc, 0, mipCount, 0, footprints.data(), rowCounts.data(), rowSizes.data(),
                                      &totalBytes);

        // 3. Copy the pixels into the staging buffer, row by row, and record one GPU copy per mip
        const UploadAllocation staging = upload.AllocateStaging(totalBytes, textureName + L" upload");
        ENGINE_ASSERT(staging.offset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0);

        CommandContext &commands = upload.Commands();
        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            const TextureMipData &source = mips[mip];
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = footprints[mip];
            const uint64_t rowSize = rowSizes[mip];
            const uint32_t rowCount = rowCounts[mip];

            // ASan cannot see GPU upload memory, so the sizes are checked here
            ENGINE_ASSERT_MSG(source.width == std::max(1u, mips[0].width >> mip) &&
                                  source.height == std::max(1u, mips[0].height >> mip),
                              "Mip {} is {}x{}, expected half of the previous level", mip, source.width, source.height);
            ENGINE_ASSERT_MSG(rowSize <= source.rowPitch && rowSize <= footprint.Footprint.RowPitch,
                              "Mip {}: row of {} bytes, source pitch {}, footprint pitch {}", mip, rowSize,
                              source.rowPitch, footprint.Footprint.RowPitch);
            ENGINE_ASSERT_MSG((rowCount - 1) * source.rowPitch + rowSize <= source.pixels.size(),
                              "Mip {}: {} rows of {} bytes do not fit in {} bytes of pixels", mip, rowCount, rowSize,
                              source.pixels.size());

            // Source rows are packed tightly, footprint rows are padded
            for (uint32_t row = 0; row < rowCount; ++row) {
                staging.WriteRange(footprint.Offset + uint64_t{row} * footprint.Footprint.RowPitch,
                                   source.pixels.subspan(row * source.rowPitch, rowSize));
            }

            // From the footprint in the buffer to mip `mip` of the texture
            footprint.Offset += staging.offset;
            commands.CopyTextureRegion(CD3DX12_TEXTURE_COPY_LOCATION(resource, mip),
                                       CD3DX12_TEXTURE_COPY_LOCATION(staging.resource, footprint));
        }
        // 4. After the copies, make it readable by pixel shaders
        commands.Transition(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // 5. A bindless slot and its view (SRV). Last, so nothing that can throw comes after taking the slot.
        texture.m_bindlessIndex = heap.Allocate();
        heap.CreateTexture2DSrv(texture.m_bindlessIndex, resource, format, mipCount);
        return texture;
    }

    void Texture::Destroy(FrameContext &frame, BindlessHeap &heap) {
        ENGINE_ASSERT_MSG(IsValid(), "Texture::Destroy on an empty texture");
        // Frames still running on the GPU may read both, so they are freed after `frame` finishes
        frame.DeferFree(heap, m_bindlessIndex);
        frame.DeferRelease(std::move(m_allocation));
        m_allocation = nullptr;
        m_bindlessIndex = 0;
    }
} // namespace engine::rhi
