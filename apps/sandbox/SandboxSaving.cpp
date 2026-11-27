#include "SandboxSaving.h"

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include "SandboxScene.h"
#include "common/GridSetup.h"
#include "core/Error.h"
#include "core/Log.h"
#include "core/Utf8.h"

namespace fs = std::filesystem;

using core::LogInfo;
using core::LogWarning;

namespace sandbox {
    namespace {
        fs::path SavePath() {
            return RepositoryRoot() / L"saves" / L"sandbox.json";
        }
    } // namespace

    bool UsesSave(const SandboxOptions &options) noexcept {
        return !options.smokeTestFrames && !options.modelPath;
    }

    std::optional<SandboxSave> ReadSave() {
        const fs::path file = SavePath();
        std::expected<SandboxSave, core::Error> save = ReadSandboxSave(file, RepositoryRoot());
        if (save) {
            LogInfo("Scene: {} ({} models, {} cubes, {} grid cubes)", core::ToUtf8(file.native()), save->models.size(),
                    save->cubes.size(), save->gridCubes.size());
            return std::move(*save);
        }
        if (save.error().code == core::ErrorCode::FileNotFound) {
            return std::nullopt;
        }
        fs::path bad = file;
        bad += L".bad";
        std::error_code ec;
        fs::rename(file, bad, ec);
        LogWarning("Scene: {}. Starting without it; the file is kept as {}", save.error().message,
                   core::ToUtf8(bad.filename().native()));
        return std::nullopt;
    }

    void RestoreNamedCubes(scene::Scene &scene, std::span<const scene::ObjectId> cubes, const SandboxSave &saved) {
        for (const SavedCube &savedCube : saved.cubes) {
            for (const scene::ObjectId id : cubes) {
                if (scene.Object(id).name == savedCube.name) {
                    scene.SetObjectWorld(id, savedCube.world);
                }
            }
        }
    }

    void WriteSave(const scene::Scene &scene, const SandboxGrid &grid, std::span<const scene::ObjectId> namedCubes,
                   const SandboxModels &models) {
        SandboxSave save{.gridCount = grid.Count(),
                         .gridMode = std::string(apps::ToString(grid.Mode())),
                         .gridCubes = grid.MovedCubes()};
        for (const scene::ObjectId id : namedCubes) {
            const scene::SceneObject &cube = scene.Object(id);
            save.cubes.push_back({.name = cube.name, .world = cube.world});
        }
        for (const PlacedModel &model : models.Placed()) {
            save.models.push_back({.path = model.path, .world = scene.Object(model.id).world});
        }

        const fs::path file = SavePath();
        if (const std::expected<void, std::string> written = WriteSandboxSave(file, save, RepositoryRoot())) {
            LogInfo("Scene saved: {}", core::ToUtf8(file.native()));
        } else {
            LogWarning("Scene not saved: {}", written.error());
        }
    }
} // namespace sandbox
