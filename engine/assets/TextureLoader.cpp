#include "assets/TextureLoader.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <format>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include <DirectXTex.h>
#include <wincodec.h>

#include "core/Assert.h"
#include "core/Utf8.h"

namespace engine::assets {
    namespace {
        using core::Error;
        using core::ErrorCode;
        using render::TextureData;
        using render::TextureMip;

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

        // Shared last part of loading: sets the format by usage, generates the mips, copies the result out
        [[nodiscard]] std::expected<TextureData, Error> FinishTexture(DirectX::ScratchImage image, TextureUsage usage,
                                                                      std::string_view source) {
            const DirectX::TexMetadata &metadata = image.GetMetadata();
            if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
                metadata.depth != 1) {
                return std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             std::format("{}: only plain 2D textures are supported", source)});
            }

            // Not WIC_FLAGS_FORCE_SRGB: that one only affects saving. Before the mips, because filtering depends on
            // it: sRGB values are averaged as light, data values as numbers.
            const DXGI_FORMAT format =
                IsSrgbUsage(usage) ? DirectX::MakeSRGB(metadata.format) : DirectX::MakeLinear(metadata.format);
            if (!image.OverrideFormat(format)) {
                return std::unexpected(
                    Error{ErrorCode::UnsupportedFormat,
                          std::format("{}: cannot use format {}", source, static_cast<uint32_t>(format))});
            }

            // Compressed formats cannot be filtered, and a 1x1 image has no smaller level
            const bool generateMips = image.GetMetadata().mipLevels == 1 && !DirectX::IsCompressed(format) &&
                                      (metadata.width > 1 || metadata.height > 1);
            if (generateMips) {
                // FORCE_NON_WIC: colour and data textures share one filter, and generating mips needs no COM
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
                    .offset = static_cast<uint64_t>(mip.pixels - image.GetPixels()),
                    .rowPitch = mip.rowPitch,
                    .slicePitch = mip.slicePitch,
                });
            }
            const auto *pixels = reinterpret_cast<const std::byte *>(image.GetPixels());
            data.pixels.assign(pixels, pixels + image.GetPixelsSize());
            return data;
        }

        [[nodiscard]] std::wstring LowerCaseExtension(const std::filesystem::path &path) {
            std::wstring extension = path.extension().wstring();
            std::ranges::transform(extension, extension.begin(),
                                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return extension;
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

    std::expected<render::TextureData, core::Error> LoadTexture(const std::filesystem::path &path, TextureUsage usage) {
        const std::string source = core::ToUtf8(path.native());
        const std::wstring extension = LowerCaseExtension(path);

        DirectX::ScratchImage image;
        HRESULT hr = S_OK;
        if (extension == L".dds") {
            hr = DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        } else if (extension == L".tga") {
            hr = DirectX::LoadFromTGAFile(path.c_str(), DirectX::TGA_FLAGS_IGNORE_SRGB, nullptr, image);
        } else {
            // IGNORE_SRGB: the usage decides, not the file's metadata. FORCE_RGB: RGBA byte order
            // instead of WIC's native BGRA.
            hr = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB | DirectX::WIC_FLAGS_FORCE_RGB,
                                          nullptr, image);
        }
        if (FAILED(hr)) {
            return std::unexpected(ErrorFromHResult(hr, source));
        }
        return FinishTexture(std::move(image), usage, source);
    }

    std::expected<render::TextureData, core::Error>
    LoadTextureFromMemory(std::span<const std::byte> bytes, std::string_view formatHint, TextureUsage usage) {
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
    CreateTextureFromPixels(std::span<const std::byte> rgba8, uint32_t width, uint32_t height, TextureUsage usage) {
        constexpr std::string_view kSource = "in-memory texture";
        constexpr size_t kBytesPerPixel = 4;
        ENGINE_ASSERT_MSG(rgba8.size() == size_t{width} * height * kBytesPerPixel,
                          "{} bytes do not match a {}x{} RGBA8 image", rgba8.size(), width, height);

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
} // namespace engine::assets
