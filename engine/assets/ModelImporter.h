#ifndef D3D12_ENGINE_MODELIMPORTER_H
#define D3D12_ENGINE_MODELIMPORTER_H

// Reads a model file into CPU data: its geometry, where each part is placed, its materials and the images stored
// inside it. Textures in separate files are recorded by path, not loaded.

#include <expected>
#include <filesystem>

#include "core/Error.h"
#include "renderdata/ModelData.h"

namespace assets {
    [[nodiscard]] std::expected<render::ModelData, core::Error> ImportModel(const std::filesystem::path &path);

    [[nodiscard]] bool IsModelFile(const std::filesystem::path &path);
} // namespace assets

#endif // D3D12_ENGINE_MODELIMPORTER_H
