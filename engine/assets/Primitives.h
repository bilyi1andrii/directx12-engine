#ifndef D3D12_ENGINE_PRIMITIVES_H
#define D3D12_ENGINE_PRIMITIVES_H

#include "renderdata/MeshData.h"

namespace engine::assets {
    // Meshes made in code, uploaded with GpuUploader::CreateMesh like imported ones

    // Cube with corners at +-1, 4 vertices per face (whole texture and own normal per face), one submesh,
    // triangles clockwise seen from outside.
    [[nodiscard]] render::MeshData MakeCube();
} // namespace engine::assets

#endif // D3D12_ENGINE_PRIMITIVES_H
