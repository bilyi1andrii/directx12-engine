#ifndef D3D12_ENGINE_MESHDATA_H
#define D3D12_ENGINE_MESHDATA_H

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Vertex.h"

namespace engine::render {
    // A part of a model drawn with one material: a range of the model's index array
    struct SubmeshData {
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t baseVertex = 0;     // first vertex of this submesh; its indices count from here
        uint32_t materialIndex = 0; // into ModelData::materials
    };

    // A model's geometry: one vertex array and one index array, shared by all its submeshes
    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubmeshData> submeshes;
    };

    // One place where a submesh is drawn inside its model. A submesh drawn in several places is still one mesh,
    // so its copies can be drawn instanced. A copy's world matrix is transform * the object's world matrix.
    struct MeshNode {
        uint32_t submesh = 0; // into MeshData::submeshes
        // Submesh -> model space, in the row-vector convention of the RenderList. Identity by default.
        DirectX::XMFLOAT4X4 transform{1.0f, 0.0f, 0.0f, 0.0f, //
                                      0.0f, 1.0f, 0.0f, 0.0f, //
                                      0.0f, 0.0f, 1.0f, 0.0f, //
                                      0.0f, 0.0f, 0.0f, 1.0f};
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MESHDATA_H
