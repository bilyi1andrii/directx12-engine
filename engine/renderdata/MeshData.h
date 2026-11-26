#ifndef D3D12_ENGINE_MESHDATA_H
#define D3D12_ENGINE_MESHDATA_H

// A model's geometry on the CPU: one vertex array and one index array shared by all its parts, and where each part
// is drawn inside the model.

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Vertex.h"

namespace render {
    struct SubmeshData {
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t baseVertex = 0;
        uint32_t materialIndex = 0;
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubmeshData> submeshes;
    };

    struct MeshNode {
        uint32_t submesh = 0;
        DirectX::XMFLOAT4X4 transform{1.0f, 0.0f, 0.0f, 0.0f, //
                                      0.0f, 1.0f, 0.0f, 0.0f, //
                                      0.0f, 0.0f, 1.0f, 0.0f, //
                                      0.0f, 0.0f, 0.0f, 1.0f};
    };
} // namespace render

#endif // D3D12_ENGINE_MESHDATA_H
