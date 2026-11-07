#ifndef D3D12_ENGINE_PRIMITIVES_H
#define D3D12_ENGINE_PRIMITIVES_H

#include "renderdata/MeshData.h"

namespace engine::assets {
    // Meshes generated in code, uploaded with GpuUploader::CreateMesh like an imported model

    // A cube with corners at +-1 and 4 vertices per face, so every face shows the whole texture and has its own
    // normal. One submesh. Triangles are clockwise seen from outside (the default front face).
    [[nodiscard]] render::MeshData MakeCube();
} // namespace engine::assets

#endif // D3D12_ENGINE_PRIMITIVES_H
