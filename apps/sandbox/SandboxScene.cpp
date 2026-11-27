#include "SandboxScene.h"

#include <algorithm>
#include <set>
#include <system_error>
#include <vector>

#include "assets/ModelImporter.h"
#include "core/File.h"

namespace fs = std::filesystem;

namespace sandbox {
    fs::path RepositoryRoot() {
        return (core::ExecutableDirectory() / L"../../..").lexically_normal();
    }

    std::vector<fs::path> FindAssetModels() {
        const fs::path root = RepositoryRoot() / L"assets";
        std::vector<fs::path> models;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            if (assets::IsModelFile(it->path())) {
                models.push_back(it->path());
            }
        }
        std::ranges::sort(models);
        return models;
    }

    void AddDefaultModels(SandboxModels &models, const SandboxOptions &options,
                          const std::optional<SandboxSave> &saved) {
        if (options.modelPath) {
            models.Add(*options.modelPath);
            return;
        }
        if (options.smokeTestFrames) {
            return;
        }
        std::set<fs::path> savedPaths;
        if (saved) {
            for (const SavedModel &model : saved->models) {
                savedPaths.insert(model.path);
            }
            models.AddSaved(saved->models);
        }
        std::vector<fs::path> newModels = FindAssetModels();
        std::erase_if(newModels,
                      [&savedPaths](const fs::path &path) { return savedPaths.contains(path.lexically_normal()); });
        models.AddRow(newModels);
    }
} // namespace sandbox
