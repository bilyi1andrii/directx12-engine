#include "assets/TextureLoader.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include <DirectXTex.h>
#include <wincodec.h>

#include "assets/Paths.h"
#include "core/Assert.h"
#include "core/Utf8.h"

namespace assets {
    namespace {
        using core::Error;
        using core::ErrorCode;
        using render::TextureData;
        using render::TextureMip;

        constexpr size_t kMaxTextureSide = 16384; // D3D12 limit

        [[nodiscard]] Error ErrorFromHResult(HRESULT hr, std::string_view source) {
            ErrorCode code = ErrorCode::DecodeFailed;
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
                code = ErrorCode::FileNotFound;
            } else if (hr == WINCODEC_ERR_COMPONENTNOTFOUND || hr == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)) {
                code = ErrorCode::UnsupportedFormat;
            }
            return Error{code,
                         std::format("{}: DirectXTex failed with HRESULT 0x{:08X}", source, static_cast<uint32_t>(hr))};
        }

        [[nodiscard]] bool IsSingleChannel(DXGI_FORMAT format) noexcept {

            return (format == DXGI_FORMAT_R8_UNORM || format == DXGI_FORMAT_R16_UNORM ||
                    format == DXGI_FORMAT_R32_FLOAT);
        }

        [[nodiscard]] std::expected<TextureData, Error> FinishTexture(DirectX::ScratchImage image, TextureUsage usage,
                                                                      std::string_view source) {
            const DirectX::TexMetadata &metadata = image.GetMetadata();
            if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
                metadata.depth != 1) {
                return std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             std::format("{}: only plain 2D textures are supported", source)});
            }
            if (metadata.width > kMaxTextureSide || metadata.height > kMaxTextureSide) {
                return std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             std::format("{}: {}x{} is over the limit of {} pixels per side", source,
                                                         metadata.width, metadata.height, kMaxTextureSide)});
            }

            // grey image as colour would read red, so expand to RGBA
            if (IsSrgbUsage(usage) && IsSingleChannel(metadata.format)) {
                DirectX::ScratchImage rgba;
                const HRESULT hr = DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                                                    DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_FORCE_NON_WIC,
                                                    DirectX::TEX_THRESHOLD_DEFAULT, rgba);
                if (FAILED(hr)) {
                    return std::unexpected(ErrorFromHResult(hr, source));
                }
                image = std::move(rgba);
            }

            // usage decides sRGB or linear, same bytes
            const DXGI_FORMAT format =
                IsSrgbUsage(usage) ? DirectX::MakeSRGB(metadata.format) : DirectX::MakeLinear(metadata.format);
            [[maybe_unused]] const bool overridden = image.OverrideFormat(format);
            ENGINE_ASSERT_MSG(overridden, "{}: DirectXTex refused format {} for the image", source,
                              static_cast<int>(format));

            // only when file has none and isn't block-compressed
            const bool generateMips = image.GetMetadata().mipLevels == 1 && !DirectX::IsCompressed(format) &&
                                      (metadata.width > 1 || metadata.height > 1);
            if (generateMips) {
                DirectX::ScratchImage withMips;
                const HRESULT hr =
                    DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), DirectX::TEX_FILTER_FORCE_NON_WIC, 0, withMips);
                if (FAILED(hr)) {
                    return std::unexpected(ErrorFromHResult(hr, source));
                }
                image = std::move(withMips);
            }

            TextureData data;
            data.dxgiFormat = static_cast<uint32_t>(image.GetMetadata().format);
            data.isSrgb = DirectX::IsSRGB(image.GetMetadata().format);
            data.mips.reserve(image.GetMetadata().mipLevels);
            for (size_t level = 0; level < image.GetMetadata().mipLevels; ++level) {
                const DirectX::Image &mip = *image.GetImage(level, 0, 0);
                data.mips.push_back(TextureMip{
                    .width = static_cast<uint32_t>(mip.width),
                    .height = static_cast<uint32_t>(mip.height),
                    .offset = static_cast<uint64_t>(mip.pixels - image.GetPixels()), // into pixels
                    .rowPitch = mip.rowPitch,
                    .slicePitch = mip.slicePitch,
                });
            }
            const auto *pixels = reinterpret_cast<const std::byte *>(image.GetPixels());
            data.pixels.assign(pixels, pixels + image.GetPixelsSize());
            return data;
        }
    } // namespace

    bool IsSrgbUsage(TextureUsage usage) noexcept {
        switch (usage) {
        case TextureUsage::Normals:
        case TextureUsage::Height:
        case TextureUsage::Opacity:
        case TextureUsage::Metalness:
        case TextureUsage::Roughness:
        case TextureUsage::AmbientOcclusion:
            return false;
        default:
            return true;
        }
    }

    std::expected<render::TextureData, core::Error> ImportTexture(const std::filesystem::path &path,
                                                                  TextureUsage usage) {
        const std::string source = core::ToUtf8(path.native());
        const std::wstring extension = LowerCaseExtension(path);

        DirectX::ScratchImage image;
        HRESULT hr = S_OK;
        // file's own sRGB flag ignored, usage decides
        if (extension == L".dds") {
            hr = DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        } else if (extension == L".tga") {
            hr = DirectX::LoadFromTGAFile(path.c_str(), DirectX::TGA_FLAGS_IGNORE_SRGB, nullptr, image);
        } else {
            hr = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB | DirectX::WIC_FLAGS_FORCE_RGB,
                                          nullptr, image);
        }
        if (FAILED(hr)) {
            return std::unexpected(ErrorFromHResult(hr, source));
        }
        return FinishTexture(std::move(image), usage, source);
    }

    std::expected<render::TextureData, core::Error>
    ImportTextureFromMemory(std::span<const std::byte> bytes, std::string_view formatHint, TextureUsage usage) {
        std::string hint(formatHint);
        std::ranges::transform(hint, hint.begin(),
                               [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
        const std::string source = std::format("embedded texture ({})", hint.empty() ? "no format hint" : hint);

        DirectX::ScratchImage image;
        HRESULT hr = S_OK;
        if (hint == "dds") {
            hr = DirectX::LoadFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        } else if (hint == "tga") {
            hr = DirectX::LoadFromTGAMemory(bytes.data(), bytes.size(), DirectX::TGA_FLAGS_IGNORE_SRGB, nullptr, image);
        } else {
            hr = DirectX::LoadFromWICMemory(bytes.data(), bytes.size(),
                                            DirectX::WIC_FLAGS_IGNORE_SRGB | DirectX::WIC_FLAGS_FORCE_RGB, nullptr,
                                            image);
        }
        if (FAILED(hr)) {
            return std::unexpected(ErrorFromHResult(hr, source));
        }
        return FinishTexture(std::move(image), usage, source);
    }

    std::expected<render::TextureData, core::Error>
    MakeTextureFromPixels(std::span<const std::byte> rgba8, uint32_t width, uint32_t height, TextureUsage usage) {
        constexpr std::string_view kSource = "in-memory texture";
        constexpr size_t kBytesPerPixel = 4;
        DirectX::ScratchImage image;
        const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
        if (FAILED(hr)) {
            return std::unexpected(ErrorFromHResult(hr, kSource));
        }
        const DirectX::Image &target = *image.GetImage(0, 0, 0);
        const size_t sourceRowPitch = size_t{width} * kBytesPerPixel;
        for (size_t y = 0; y < height; ++y) {
            std::memcpy(target.pixels + y * target.rowPitch, rgba8.data() + y * sourceRowPitch, sourceRowPitch);
        }
        return FinishTexture(std::move(image), usage, kSource);
    }
} // namespace assets
