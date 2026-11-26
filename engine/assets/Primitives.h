#ifndef D3D12_ENGINE_PRIMITIVES_H
#define D3D12_ENGINE_PRIMITIVES_H

// Meshes and textures made in code: the cube, the sphere and the checkerboard, uploaded like imported ones.

#include <cstdint>
#include <expected>

#include "core/Error.h"
#include "renderdata/MeshData.h"
#include "renderdata/Model.h"
#include "renderdata/TextureData.h"

namespace assets {

    inline constexpr render::Aabb kCubeBounds{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    inline constexpr uint64_t kCubeTriangleCount = 12;

    [[nodiscard]] render::MeshData MakeCube();

    inline constexpr render::Aabb kSphereBounds{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    inline constexpr uint32_t kSphereSlices = 32;
    inline constexpr uint32_t kSphereStacks = 16;
    inline constexpr uint64_t kSphereTriangleCount = uint64_t{kSphereSlices} * 2 * (kSphereStacks - 1);

    [[nodiscard]] render::MeshData MakeSphere();

    [[nodiscard]] std::expected<render::TextureData, core::Error> MakeCheckerboard();
} // namespace assets

#endif // D3D12_ENGINE_PRIMITIVES_H
