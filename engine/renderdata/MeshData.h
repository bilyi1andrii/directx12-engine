#ifndef D3D12_ENGINE_MESHDATA_H
#define D3D12_ENGINE_MESHDATA_H

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Vertex.h"

namespace engine::render {
    // One part of a model drawn with one material (one aiMesh): a range of the model's shared index array.
    // The renderer turns each submesh into its own MeshHandle.
    struct SubmeshData {
        uint32_t firstIndex = 0; // into MeshData::indices
        uint32_t indexCount = 0;
        int32_t baseVertex = 0;     // added to each index: where this submesh's vertices start in MeshData::vertices
        uint32_t materialIndex = 0; // into ModelData::materials
    };

    // The geometry of a whole model file, in model space: one vertex and one index array shared by all
    // submeshes. CPU only; Renderer::CreateMesh copies it to the GPU.
    struct MeshData {
        std::vector<Vertex> vertices;
        // 32-bit for simplicity. Indices are relative to their submesh's baseVertex, so the renderer may
        // narrow them to 16-bit later.
        std::vector<uint32_t> indices;
        std::vector<SubmeshData> submeshes;
    };

    // Where a submesh is placed in its model: one per (node, mesh) pair of the model file. Kept apart from the
    // geometry, so a mesh used by several nodes stays one MeshHandle and its copies can be drawn instanced.
    // World matrix of a copy = transform * the entity's world (DX11 GeometrySystem: mesh transform * object world).
    struct MeshNode {
        uint32_t submesh = 0; // into MeshData::submeshes
        // Node -> model space, accumulated over the parent nodes. DirectXMath row-vector convention, like the
        // RenderList worlds: Assimp's matrix transposed (DX11 Model::ProcessNode). Identity by default.
        DirectX::XMFLOAT4X4 transform{1.0f, 0.0f, 0.0f, 0.0f, //
                                      0.0f, 1.0f, 0.0f, 0.0f, //
                                      0.0f, 0.0f, 1.0f, 0.0f, //
                                      0.0f, 0.0f, 0.0f, 1.0f};
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MESHDATA_H
