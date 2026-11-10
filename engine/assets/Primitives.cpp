#include "assets/Primitives.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "assets/TextureLoader.h"

namespace engine::assets {
    render::MeshData MakeCube() {
        using DirectX::XMFLOAT3;
        // Each face as seen from outside: its normal and which way is right and up on it
        struct Face {
            XMFLOAT3 normal;
            XMFLOAT3 right;
            XMFLOAT3 up;
        };
        constexpr std::array<Face, 6> kFaces{{
            {{0, 0, -1}, {1, 0, 0}, {0, 1, 0}},  // -Z
            {{0, 0, 1}, {-1, 0, 0}, {0, 1, 0}},  // +Z
            {{-1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, // -X
            {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}},   // +X
            {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},   // +Y, seen from above with +Z up
            {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}}, // -Y, seen from below with -Z up
        }};
        // A corner of the face: r and u are -1 or +1
        const auto corner = [](const Face &face, float r, float u) {
            return XMFLOAT3{face.normal.x + r * face.right.x + u * face.up.x,
                            face.normal.y + r * face.right.y + u * face.up.y,
                            face.normal.z + r * face.right.z + u * face.up.z};
        };

        render::MeshData cube;
        cube.vertices.reserve(kFaces.size() * 4);
        cube.indices.reserve(kFaces.size() * 6);
        for (const Face &face : kFaces) {
            const auto first = static_cast<uint32_t>(cube.vertices.size());
            // Bottom-left, top-left, top-right, bottom-right. UV (0, 0) is the top-left of the texture.
            cube.vertices.push_back({corner(face, -1, -1), {0, 1}, face.normal, face.right});
            cube.vertices.push_back({corner(face, -1, 1), {0, 0}, face.normal, face.right});
            cube.vertices.push_back({corner(face, 1, 1), {1, 0}, face.normal, face.right});
            cube.vertices.push_back({corner(face, 1, -1), {1, 1}, face.normal, face.right});
            // Two clockwise triangles
            cube.indices.insert(cube.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
        }
        cube.submeshes.push_back({.firstIndex = 0, .indexCount = static_cast<uint32_t>(cube.indices.size())});
        return cube;
    }

    std::expected<render::TextureData, core::Error> MakeCheckerboard() {
        constexpr uint32_t kSize = 256;
        constexpr uint32_t kSquare = 32;
        std::vector<std::byte> rgba8(size_t{kSize} * kSize * 4);
        for (uint32_t y = 0; y < kSize; ++y) {
            for (uint32_t x = 0; x < kSize; ++x) {
                const bool light = (x / kSquare + y / kSquare) % 2 == 0;
                const std::byte value{light ? uint8_t{220} : uint8_t{60}};
                std::byte *pixel = &rgba8[(size_t{y} * kSize + x) * 4];
                pixel[0] = value;
                pixel[1] = value;
                pixel[2] = value;
                pixel[3] = std::byte{255};
            }
        }
        return MakeTextureFromPixels(rgba8, kSize, kSize, TextureUsage::Diffuse);
    }
} // namespace engine::assets
