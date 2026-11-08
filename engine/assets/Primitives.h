#ifndef D3D12_ENGINE_PRIMITIVES_H
#define D3D12_ENGINE_PRIMITIVES_H

#include <cstdint>
#include <expected>

#include "core/Error.h"
#include "renderdata/MeshData.h"
#include "renderdata/Model.h"
#include "renderdata/TextureData.h"

namespace engine::assets {
    // Meshes and textures made in code, uploaded through GpuUploader like imported ones

    // The box around MakeCube's vertices
    inline constexpr render::Aabb kCubeBounds{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    // Two per face of MakeCube's cube
    inline constexpr uint64_t kCubeTriangleCount = 12;

    // Cube with corners at +-1, 4 vertices per face (whole texture and own normal per face), one submesh,
    // triangles clockwise seen from outside.
    [[nodiscard]] render::MeshData MakeCube();

    // A 256x256 light and dark grey checkerboard, as a colour texture. Without mips, distant cubes would shimmer.
    [[nodiscard]] std::expected<render::TextureData, core::Error> MakeCheckerboard();
} // namespace engine::assets

#endif // D3D12_ENGINE_PRIMITIVES_H
