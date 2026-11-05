#ifndef D3D12_ENGINE_MODELDATA_H
#define D3D12_ENGINE_MODELDATA_H

#include <cstddef>
#include <string>
#include <vector>

#include "renderdata/MaterialDesc.h"
#include "renderdata/MeshData.h"

namespace engine::render {
    // A compressed image stored inside a model file (.png/.jpg/.dds inside .glb/.fbx), not decoded yet.
    // Uncompressed embedded images are not supported (DX11 did not support them either).
    struct EmbeddedTextureData {
        std::vector<std::byte> bytes;
        std::string formatHint; // aiTexture::achFormatHint ("png", "jpg", "dds", ...): picks the decoder
    };

    // Everything read from one model file: what the model importer returns. CPU only.
    struct ModelData {
        MeshData mesh;
        std::vector<MeshNode> nodes; // where each submesh is drawn; a submesh may appear in several nodes
        std::vector<MaterialDesc> materials;
        std::vector<EmbeddedTextureData> embeddedTextures; // EmbeddedTexture::index points here
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MODELDATA_H
