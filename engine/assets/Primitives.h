#ifndef D3D12_ENGINE_PRIMITIVES_H
#define D3D12_ENGINE_PRIMITIVES_H

#include "renderdata/MeshData.h"
#include "renderdata/Model.h"

namespace engine::assets {
    // Meshes made in code, uploaded with GpuUploader::CreateMesh like imported ones

    // The box around MakeCube's vertices
    inline constexpr render::Aabb kCubeBounds{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};

    // Cube with corners at +-1, 4 vertices per face (whole texture and own normal per face), one submesh,
    // triangles clockwise seen from outside.
    [[nodiscard]] render::MeshData MakeCube();
} // namespace engine::assets

#endif // D3D12_ENGINE_PRIMITIVES_H
