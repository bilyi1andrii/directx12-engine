#ifndef D3D12_ENGINE_MODELDATA_H
#define D3D12_ENGINE_MODELDATA_H

// Everything loaded from one model file: its geometry, its materials 
// and the images stored inside it, still compressed.

#include <cstddef>
#include <string>
#include <vector>

#include "renderdata/MaterialDesc.h"
#include "renderdata/MeshData.h"

namespace render {
    struct EmbeddedTextureData {
        std::vector<std::byte> bytes;
        std::string formatHint;
    };

    struct ModelData {
        MeshData mesh;
        std::vector<MeshNode> nodes;
        std::vector<MaterialDesc> materials;
        std::vector<EmbeddedTextureData> embeddedTextures;
    };
} // namespace render

#endif // D3D12_ENGINE_MODELDATA_H
