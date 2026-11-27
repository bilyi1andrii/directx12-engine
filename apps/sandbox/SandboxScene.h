#ifndef D3D12_ENGINE_SANDBOXSCENE_H
#define D3D12_ENGINE_SANDBOXSCENE_H

// The sandbox's default models: the one given on the command line, or the saved ones and every new model in the
// repository's assets/ folder.

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "SandboxModels.h"
#include "SandboxOptions.h"
#include "sandbox/SceneSave.h"

namespace sandbox {
    inline constexpr std::wstring_view kSpinningCubeTexture = L"data/textures/diamante.jpg";

    [[nodiscard]] std::filesystem::path RepositoryRoot();

    [[nodiscard]] std::vector<std::filesystem::path> FindAssetModels();

    void AddDefaultModels(SandboxModels &models, const SandboxOptions &options,
                          const std::optional<SandboxSave> &saved);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSCENE_H
