#ifndef D3D12_ENGINE_MODELDATA_H
#define D3D12_ENGINE_MODELDATA_H

#include <cstddef>
#include <string>
#include <vector>

#include "renderdata/MaterialDesc.h"
#include "renderdata/MeshData.h"

namespace render {
    // An image stored inside a model file, still compressed (.png, .jpg, .dds)
    struct EmbeddedTextureData {
        std::vector<std::byte> bytes;
        std::string formatHint; // file type, e.g. "png"; picks the decoder
    };

    // Everything loaded from one model file
    struct ModelData {
        MeshData mesh;
        std::vector<MeshNode> nodes; // where each submesh is drawn
        std::vector<MaterialDesc> materials;
        std::vector<EmbeddedTextureData> embeddedTextures;
    };
} // namespace render

#endif // D3D12_ENGINE_MODELDATA_H
