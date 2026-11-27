#include "SandboxScene.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "SandboxLights.h"
#include "assets/ModelImporter.h"
#include "assets/TextureLoader.h"
#include "core/File.h"

namespace fs = std::filesystem;

namespace sandbox {
    namespace {
        constexpr std::wstring_view kSpinningCubeTexture = L"data/textures/diamante.jpg";

        scene::ObjectId AddCube(scene::Scene &scene, assets::AssetCache &assets, std::string name,
                                render::TextureHandle texture, float x, float z) {
            DirectX::XMFLOAT4X4 world{};
            DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(x, 0.0f, z));
            return scene.AddObject(std::move(name), assets.CubeModel(texture), world, scene::Mobility::Movable);
        }
    } // namespace

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

    SandboxCubes AddCubesAndLights(scene::Scene &scene, assets::AssetCache &assets, bool besideModels) {
        SandboxCubes cubes;
        if (!besideModels) {
            cubes.spinning = AddCube(
                scene, assets, "Spinning cube",
                assets.LoadTexture(core::ExecutableDirectory() / kSpinningCubeTexture, assets::TextureUsage::Diffuse),
                0.0f, 0.0f);
        }
        const float z = besideModels ? kModelSize / 2 + 3.0f : 0.0f;
        cubes.named = {
            AddCube(scene, assets, "Orange cube", assets.SolidColor(230, 120, 30), -5.0f, z),
            AddCube(scene, assets, "Green cube", assets.SolidColor(60, 170, 70), 5.0f, z),
            AddCube(scene, assets, "Blue cube", assets.SolidColor(50, 110, 220), 0.0f, z + 5.0f),
        };
        AddDefaultLights(scene, assets, z);
        return cubes;
    }
} // namespace sandbox
