#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "TestFiles.h"
#include "assets/TextureLoader.h"
#include "core/Utf8.h"

using engine::assets::CreateTextureFromPixels;
using engine::assets::IsSrgbUsage;
using engine::assets::LoadTexture;
using engine::assets::TextureUsage;
using engine::render::TextureData;
using engine::core::ErrorCode;
using engine::tests::MissingFile;
using engine::tests::TempFile;

namespace {
    // DXGI_FORMAT values: the tests, like the assets layer, do not include DXGI headers
    constexpr uint32_t kRgba8Unorm = 28;
    constexpr uint32_t kRgba8Srgb = 29;
    constexpr uint32_t kBc1Unorm = 71;
    constexpr uint32_t kBc1Srgb = 72;

    // Black-and-white RGBA8 checkerboard, opaque
    [[nodiscard]] std::vector<std::byte> Checkerboard(uint32_t width, uint32_t height) {
        std::vector<std::byte> pixels;
        pixels.reserve(size_t{width} * height * 4);
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const auto value = std::byte{(x + y) % 2 == 0 ? uint8_t{255} : uint8_t{0}};
                pixels.insert(pixels.end(), {value, value, value, std::byte{255}});
            }
        }
        return pixels;
    }

    [[nodiscard]] int RedOfFirstPixelInLastMip(const TextureData &data) {
        return std::to_integer<int>(data.pixels[data.mips.back().offset]);
    }

    // Uncompressed 32-bit TGA: 18-byte header plus BGRA pixels
    [[nodiscard]] std::vector<std::byte> Tga2x2() {
        std::vector<std::byte> file(18 + 2 * 2 * 4, std::byte{255});
        std::memset(file.data(), 0, 18);
        file[2] = std::byte{2};         // uncompressed true colour
        file[12] = std::byte{2};        // width
        file[14] = std::byte{2};        // height
        file[16] = std::byte{32};       // bits per pixel
        file[17] = std::byte{8 | 0x20}; // 8 alpha bits, top-left origin
        return file;
    }

    // 4x4 DDS with one BC1 mip: magic, 124-byte header, one 8-byte block
    [[nodiscard]] std::vector<std::byte> DdsBc1() {
        std::array<uint32_t, 32> header{};
        header[0] = 0x20534444;               // "DDS "
        header[1] = 124;                      // header size
        header[2] = 0x1 | 0x2 | 0x4 | 0x1000; // caps, height, width, pixel format
        header[3] = 4;                        // height
        header[4] = 4;                        // width
        header[7] = 1;                        // mip count
        header[19] = 32;                      // pixel format size
        header[20] = 0x4;                     // pixel format has a FourCC
        header[21] = 0x31545844;              // "DXT1"
        header[27] = 0x1000;                  // caps: texture
        std::vector<std::byte> file(sizeof(header) + 8, std::byte{0});
        std::memcpy(file.data(), header.data(), sizeof(header));
        return file;
    }
} // namespace

// DX11 rule (Graphics/Texture.cpp): colour is sRGB, data is not, and anything else (`default:`) is sRGB
TEST(TextureUsage, SrgbRuleMatchesDx11) {
    for (const TextureUsage usage : {TextureUsage::Diffuse, TextureUsage::BaseColor, TextureUsage::Emissive,
                                     TextureUsage::Specular, TextureUsage::Unknown}) {
        EXPECT_TRUE(IsSrgbUsage(usage)) << static_cast<int>(usage);
    }
    for (const TextureUsage usage :
         {TextureUsage::Normals, TextureUsage::Height, TextureUsage::Opacity, TextureUsage::Metalness,
          TextureUsage::Roughness, TextureUsage::AmbientOcclusion}) {
        EXPECT_FALSE(IsSrgbUsage(usage)) << static_cast<int>(usage);
    }
}

// sRGB bytes mean light: black + white average to 0.5 linear, which encodes to about 188, not 128
TEST(CreateTextureFromPixels, SrgbMipsAverageLight) {
    const auto result = CreateTextureFromPixels(Checkerboard(4, 4), 4, 4, TextureUsage::Diffuse);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->dxgiFormat, kRgba8Srgb);
    EXPECT_TRUE(result->isSrgb);
    ASSERT_EQ(result->mips.size(), 3u);
    EXPECT_NEAR(RedOfFirstPixelInLastMip(*result), 188, 3);
}

// Data bytes are plain numbers: the average of 0 and 255 is about 128
TEST(CreateTextureFromPixels, DataMipsAverageNumbers) {
    const auto result = CreateTextureFromPixels(Checkerboard(4, 4), 4, 4, TextureUsage::Roughness);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->dxgiFormat, kRgba8Unorm);
    EXPECT_FALSE(result->isSrgb);
    ASSERT_EQ(result->mips.size(), 3u);
    EXPECT_NEAR(RedOfFirstPixelInLastMip(*result), 128, 2);
}

TEST(CreateTextureFromPixels, MipTableDescribesEveryLevel) {
    const auto result = CreateTextureFromPixels(Checkerboard(8, 4), 8, 4, TextureUsage::Diffuse);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result->mips.size(), 4u);
    constexpr std::array<std::array<uint32_t, 2>, 4> kSizes{{{8, 4}, {4, 2}, {2, 1}, {1, 1}}};
    uint64_t nextFreeByte = 0;
    for (size_t level = 0; level < kSizes.size(); ++level) {
        const auto &mip = result->mips[level];
        EXPECT_EQ(mip.width, kSizes[level][0]);
        EXPECT_EQ(mip.height, kSizes[level][1]);
        EXPECT_EQ(mip.rowPitch, uint64_t{mip.width} * 4);
        EXPECT_EQ(mip.slicePitch, mip.rowPitch * mip.height);
        EXPECT_GE(mip.offset, nextFreeByte) << "level " << level << " overlaps the previous one";
        nextFreeByte = mip.offset + mip.slicePitch;
    }
    EXPECT_LE(nextFreeByte, result->pixels.size());
}

TEST(CreateTextureFromPixels, NonPowerOfTwoGetsAFullChain) {
    const auto result = CreateTextureFromPixels(Checkerboard(5, 3), 5, 3, TextureUsage::Roughness);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result->mips.size(), 3u);
    EXPECT_EQ(result->mips[1].width, 2u);
    EXPECT_EQ(result->mips[1].height, 1u);
    EXPECT_EQ(result->mips[2].width, 1u);
    EXPECT_EQ(result->mips[2].height, 1u);
}

// The DX11 fallback textures are 1x1, and GenerateMipMaps fails on 1x1 images
TEST(CreateTextureFromPixels, OneByOneHasOneLevel) {
    const std::array pixel{std::byte{100}, std::byte{100}, std::byte{100}, std::byte{255}};

    const auto result = CreateTextureFromPixels(pixel, 1, 1, TextureUsage::Diffuse);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result->mips.size(), 1u);
    EXPECT_EQ(result->pixels[0], std::byte{100});
}

// Also covers an upper-case extension and a path outside the ANSI code page
TEST(LoadTexture, DecodesTga) {
    const TempFile file(L"engine_test_текстура.TGA", Tga2x2());

    const auto result = LoadTexture(file.Path(), TextureUsage::Diffuse);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->dxgiFormat, kRgba8Srgb);
    EXPECT_EQ(result->mips.size(), 2u);
}

TEST(LoadTexture, CompressedDdsKeepsItsLevelsAndFollowsUsage) {
    const TempFile file(L"engine_test_bc1.dds", DdsBc1());

    const auto colour = LoadTexture(file.Path(), TextureUsage::Diffuse);
    ASSERT_TRUE(colour.has_value()) << colour.error().message;
    EXPECT_EQ(colour->dxgiFormat, kBc1Srgb);
    EXPECT_EQ(colour->mips.size(), 1u);

    const auto data = LoadTexture(file.Path(), TextureUsage::Normals);
    ASSERT_TRUE(data.has_value()) << data.error().message;
    EXPECT_EQ(data->dxgiFormat, kBc1Unorm);
}

// The .png case goes through WIC: without COM it fails with DecodeFailed instead.
// The message names the file in UTF-8, also outside the ANSI code page (see core/File.cpp).
TEST(LoadTexture, MissingFileReturnsFileNotFound) {
    for (const wchar_t *name : {L"engine_test_нема.png", L"engine_test_missing.dds", L"engine_test_missing.tga"}) {
        const auto result = LoadTexture(MissingFile(name), TextureUsage::Diffuse);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::FileNotFound) << result.error().message;
        EXPECT_NE(result.error().message.find(engine::core::ToUtf8(name)), std::string::npos) << result.error().message;
    }
}

// Goes through WIC, so it also fails if COM is not started
TEST(LoadTexture, FileThatIsNotAnImageReturnsUnsupportedFormat) {
    constexpr std::array garbage{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const TempFile file(L"engine_test_garbage.png", garbage);

    const auto result = LoadTexture(file.Path(), TextureUsage::Diffuse);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::UnsupportedFormat) << result.error().message;
}
