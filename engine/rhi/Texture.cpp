#include "rhi/Texture.h"

#include <array>
#include <exception>
#include <string>
#include <utility>

#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"
#include "rhi/BindlessHeap.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadContext.h"
#include "rhi/UploadRing.h"

namespace rhi {
    using core::ThrowIfFailed;

    Texture::~Texture() {
        ENGINE_ASSERT_MSG(!IsValid() || std::uncaught_exceptions() > 0,
                          "Texture with bindless index {} destroyed without Destroy()", m_bindlessIndex);
    }

    Texture Texture::Create(Allocator &allocator, UploadContext &upload, BindlessHeap &heap, DXGI_FORMAT format,
                            std::span<const TextureMipData> mips, std::wstring_view name) {
        const auto mipCount = static_cast<uint32_t>(mips.size());
        const D3D12_RESOURCE_DESC desc =
            CD3DX12_RESOURCE_DESC::Tex2D(format, mips[0].width, mips[0].height, 1, static_cast<UINT16>(mipCount));

        Texture texture;
        const std::wstring textureName(name);
        texture.m_allocation = allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, textureName);
        ID3D12Resource *resource = texture.Native();

        std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, D3D12_REQ_MIP_LEVELS> footprints{};
        std::array<UINT, D3D12_REQ_MIP_LEVELS> rowCounts{};
        std::array<UINT64, D3D12_REQ_MIP_LEVELS> rowSizes{};
        UINT64 totalBytes = 0;
        ComPtr<ID3D12Device> device;
        ThrowIfFailed(resource->GetDevice(IID_PPV_ARGS(&device)));
        device->GetCopyableFootprints(&desc, 0, mipCount, 0, footprints.data(), rowCounts.data(), rowSizes.data(),
                                      &totalBytes);

        const UploadAllocation staging = upload.AllocateStaging(totalBytes, textureName + L" upload");

        CommandContext &commands = upload.Commands();
        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            const TextureMipData &source = mips[mip];
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = footprints[mip];
            const uint64_t rowSize = rowSizes[mip];
            const uint32_t rowCount = rowCounts[mip];

            ENGINE_ASSERT_MSG((rowCount - 1) * source.rowPitch + rowSize <= source.pixels.size(),
                              "Mip {}: {} rows of {} bytes do not fit in {} bytes of pixels", mip, rowCount, rowSize,
                              source.pixels.size());

            for (uint32_t row = 0; row < rowCount; ++row) {
                staging.WriteRange(footprint.Offset + uint64_t{row} * footprint.Footprint.RowPitch,
                                   source.pixels.subspan(row * source.rowPitch, rowSize));
            }

            footprint.Offset += staging.offset;
            commands.CopyTextureRegion(CD3DX12_TEXTURE_COPY_LOCATION(resource, mip),
                                       CD3DX12_TEXTURE_COPY_LOCATION(staging.resource, footprint));
        }
        commands.Transition(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        texture.m_bindlessIndex = heap.Allocate();
        heap.CreateTexture2DSrv(texture.m_bindlessIndex, resource, format, mipCount);
        return texture;
    }

    void Texture::Destroy(FrameContext &frame, BindlessHeap &heap) {
        frame.DeferFree(heap, m_bindlessIndex);
        frame.DeferRelease(std::move(m_allocation));
        m_allocation = nullptr;
        m_bindlessIndex = 0;
    }
} // namespace rhi
