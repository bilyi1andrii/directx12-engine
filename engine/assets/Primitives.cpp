#include "assets/Primitives.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "assets/TextureLoader.h"

namespace assets {
    render::MeshData MakeCube() {
        using DirectX::XMFLOAT3;
        struct Face {
            XMFLOAT3 normal;
            XMFLOAT3 right;
            XMFLOAT3 up;
        };
        constexpr std::array<Face, 6> kFaces{{
            {{0, 0, -1}, {1, 0, 0}, {0, 1, 0}},
            {{0, 0, 1}, {-1, 0, 0}, {0, 1, 0}},
            {{-1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
            {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},
            {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}},
        }};
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
            cube.vertices.push_back({corner(face, -1, -1), {0, 1}, face.normal, face.right});
            cube.vertices.push_back({corner(face, -1, 1), {0, 0}, face.normal, face.right});
            cube.vertices.push_back({corner(face, 1, 1), {1, 0}, face.normal, face.right});
            cube.vertices.push_back({corner(face, 1, -1), {1, 1}, face.normal, face.right});
            cube.indices.insert(cube.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
        }
        cube.submeshes.push_back({.firstIndex = 0, .indexCount = static_cast<uint32_t>(cube.indices.size())});
        return cube;
    }

    render::MeshData MakeSphere() {
        using DirectX::XMFLOAT2;
        using DirectX::XMFLOAT3;

        render::MeshData sphere;
        const uint32_t columns = kSphereSlices + 1;
        sphere.vertices.reserve(size_t{kSphereStacks + 1} * columns);
        for (uint32_t stack = 0; stack <= kSphereStacks; ++stack) {
            const float v = static_cast<float>(stack) / kSphereStacks;
            const float fromTop = DirectX::XM_PI * v;
            const float y = std::cos(fromTop);
            const float ringRadius = std::sin(fromTop);
            for (uint32_t slice = 0; slice <= kSphereSlices; ++slice) {
                const float u = static_cast<float>(slice) / kSphereSlices;
                const float around = DirectX::XM_2PI * u;
                const float cosAround = std::cos(around);
                const float sinAround = std::sin(around);
                const XMFLOAT3 position{ringRadius * cosAround, y, ringRadius * sinAround};
                sphere.vertices.push_back({position, XMFLOAT2{u, v}, position, XMFLOAT3{-sinAround, 0.0f, cosAround}});
            }
        }

        sphere.indices.reserve(kSphereTriangleCount * 3);
        for (uint32_t stack = 0; stack < kSphereStacks; ++stack) {
            for (uint32_t slice = 0; slice < kSphereSlices; ++slice) {
                const uint32_t topLeft = stack * columns + slice;
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = topLeft + columns;
                const uint32_t bottomRight = bottomLeft + 1;
                if (stack + 1 < kSphereStacks) {
                    sphere.indices.insert(sphere.indices.end(), {topLeft, bottomRight, bottomLeft});
                }
                if (stack > 0) {
                    sphere.indices.insert(sphere.indices.end(), {topLeft, topRight, bottomRight});
                }
            }
        }
        sphere.submeshes.push_back({.firstIndex = 0, .indexCount = static_cast<uint32_t>(sphere.indices.size())});
        return sphere;
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
} // namespace assets
