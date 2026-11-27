#ifndef D3D12_ENGINE_SANDBOXSCENE_H
#define D3D12_ENGINE_SANDBOXSCENE_H

// The sandbox's default scene: the model given on the command line, or the saved models and every new one in the
// repository's assets/ folder, then the coloured cubes and the lights in front of them.

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include "SandboxModels.h"
#include "SandboxOptions.h"
#include "assets/AssetCache.h"
#include "sandbox/SceneSave.h"
#include "scene/Scene.h"

namespace sandbox {
    struct SandboxCubes {
        std::optional<scene::ObjectId> spinning;
        std::array<scene::ObjectId, 3> named{};
    };

    [[nodiscard]] std::filesystem::path RepositoryRoot();

    [[nodiscard]] std::vector<std::filesystem::path> FindAssetModels();

    void AddDefaultModels(SandboxModels &models, const SandboxOptions &options,
                          const std::optional<SandboxSave> &saved);

    [[nodiscard]] SandboxCubes AddCubesAndLights(scene::Scene &scene, assets::AssetCache &assets, bool besideModels);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSCENE_H
