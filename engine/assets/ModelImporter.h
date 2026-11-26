#ifndef D3D12_ENGINE_MODELIMPORTER_H
#define D3D12_ENGINE_MODELIMPORTER_H

#include <expected>
#include <filesystem>

#include "core/Error.h"
#include "renderdata/ModelData.h"

namespace assets {
    // Reads a model file (.obj, .gltf, .glb, .fbx, ... anything Assimp reads) into CPU data: geometry, where each
    // submesh is placed, materials and the textures stored inside the file. Textures in separate files are not
    // loaded, only their paths are recorded.
    [[nodiscard]] std::expected<render::ModelData, core::Error> ImportModel(const std::filesystem::path &path);

    [[nodiscard]] bool IsModelFile(const std::filesystem::path &path);
} // namespace assets

#endif // D3D12_ENGINE_MODELIMPORTER_H
