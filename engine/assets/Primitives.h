#ifndef D3D12_ENGINE_PRIMITIVES_H
#define D3D12_ENGINE_PRIMITIVES_H

#include <cstdint>
#include <expected>

#include "core/Error.h"
#include "renderdata/MeshData.h"
#include "renderdata/Model.h"
#include "renderdata/TextureData.h"

namespace assets {
    // Meshes and textures made in code, uploaded through GpuUploader like imported ones

    // The box around MakeCube's vertices
    inline constexpr render::Aabb kCubeBounds{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    // Two per face of MakeCube's cube
    inline constexpr uint64_t kCubeTriangleCount = 12;

    // Cube with corners at +-1, 4 vertices per face (whole texture and own normal per face), one submesh,
    // triangles clockwise seen from outside.
    [[nodiscard]] render::MeshData MakeCube();

    // The box around MakeSphere's vertices
    inline constexpr render::Aabb kSphereBounds{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    // MakeSphere's grid: steps around the Y axis, and steps from the top pole to the bottom one
    inline constexpr uint32_t kSphereSlices = 32;
    inline constexpr uint32_t kSphereStacks = 16;
    // Two per grid cell, one per cell that touches a pole
    inline constexpr uint64_t kSphereTriangleCount = uint64_t{kSphereSlices} * 2 * (kSphereStacks - 1);

    // Sphere of radius 1 around the origin with smooth normals, one submesh, triangles clockwise seen from
    // outside. A texture wraps once around the Y axis, its top edge at the +Y pole.
    [[nodiscard]] render::MeshData MakeSphere();

    // A 256x256 light and dark grey checkerboard, as a colour texture. Without mips, distant cubes would shimmer.
    [[nodiscard]] std::expected<render::TextureData, core::Error> MakeCheckerboard();
} // namespace assets

#endif // D3D12_ENGINE_PRIMITIVES_H
