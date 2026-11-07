#include "SandboxScene.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <DirectXMath.h>

#include "assets/ModelImporter.h"
#include "assets/TextureLoader.h"
#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "core/Utf8.h"

using engine::core::LogInfo;
using engine::core::LogWarning;

namespace sandbox {
    namespace {
        // `path` as a colour texture named `name`; grey with a warning on failure
        engine::render::TextureHandle LoadTextureOrGrey(engine::render::Renderer &renderer,
                                                        const std::filesystem::path &path, std::wstring_view name) {
            const std::expected<engine::render::TextureData, engine::core::Error> texture =
                engine::assets::LoadTexture(path, engine::assets::TextureUsage::Diffuse);
            if (!texture) {
                LogWarning("Texture {}: {}. Using the grey fallback.", engine::core::ToUtf8(name),
                           texture.error().message);
                return renderer.UnloadedTexture();
            }
            return renderer.CreateTexture(*texture, name);
        }
    } // namespace

    engine::render::TextureHandle CreateSpinningCubeTexture(engine::render::Renderer &renderer) {
        // The build copies data/ next to the exe
        const std::filesystem::path path = engine::core::ExecutableDirectory() / kSpinningCubeTexture;
        return LoadTextureOrGrey(renderer, path, L"SpinningCube/" + path.filename().wstring());
    }

    std::vector<std::filesystem::path> FindAssetModels() {
        const std::filesystem::path root =
            (engine::core::ExecutableDirectory() / L"../../../assets").lexically_normal();
        std::vector<std::filesystem::path> models;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (engine::assets::IsModelFile(it->path())) {
                models.push_back(it->path());
            }
        }
        std::ranges::sort(models);
        return models;
    }

    bool AddModel(engine::scene::Scene &scene, engine::assets::AssetCache &assets, const std::filesystem::path &path,
                  float x, float z) {
        using namespace DirectX;

        const auto model = assets.LoadModel(path);
        if (!model) {
            LogWarning("Model: {}", model.error().message);
            return false;
        }
        const engine::render::Aabb &bounds = (*model)->bounds;
        const float extent =
            std::max({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z, 1e-6f});
        const float scale = kModelSize / extent;
        // Row vectors: centre with the bottom at 0, scale, move to (x, kGridTop, z)
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixTranslation(-(bounds.min.x + bounds.max.x) / 2, -bounds.min.y,
                                                    -(bounds.min.z + bounds.max.z) / 2) *
                                    XMMatrixScaling(scale, scale, scale) *
                                    XMMatrixTranslation(x, engine::apps::kGridTop, z));
        scene.Instantiate(**model, world);
        LogInfo("Model {}: {} parts, largest side {:.2f} -> scaled by {:.4f}",
                engine::core::ToUtf8((path.parent_path().filename() / path.filename()).generic_wstring()),
                (*model)->parts.size(), extent, scale);
        return true;
    }

    size_t AddModels(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                     std::span<const std::filesystem::path> paths) {
        size_t added = 0;
        for (size_t i = 0; i < paths.size(); ++i) {
            const float x = (static_cast<float>(i) - static_cast<float>(paths.size() - 1) / 2) * kModelSpacing;
            added += AddModel(scene, assets, paths[i], x, 0.0f) ? 1 : 0;
        }
        return added;
    }

    void LoadDroppedFile(const std::filesystem::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint,
                         engine::render::Renderer &renderer, engine::assets::AssetCache &assets,
                         engine::scene::Scene &scene, float &nextModelX, engine::apps::StressGrid &grid) {
        engine::core::Timer timer;
        timer.Start();
        const std::string name = engine::core::ToUtf8(path.filename().wstring());
        LogInfo("Dropped {}: loading...", name);

        if (engine::assets::IsModelFile(path)) {
            const float x = dropPoint ? dropPoint->x : nextModelX;
            const float z = dropPoint ? dropPoint->z : 0.0f;
            if (AddModel(scene, assets, path, x, z)) {
                if (!dropPoint) {
                    nextModelX += kModelSpacing;
                }
                LogInfo("Dropped {}: loaded in {:.0f} ms at ({:.1f}, {:.1f})", name, timer.ElapsedMs(), x, z);
            }
            return;
        }
        if (grid.Mode() != engine::apps::GridMode::Dynamic) {
            LogWarning("Dropped {}: a static grid keeps its texture (start with --grid dynamic)", name);
            return;
        }
        const std::expected<engine::render::TextureData, engine::core::Error> texture =
            engine::assets::LoadTexture(path, engine::assets::TextureUsage::Diffuse);
        if (!texture) {
            LogWarning("Dropped {}: neither a model (.gltf, .glb, .obj, .fbx) nor a readable image: {}", name,
                       texture.error().message);
            return;
        }
        grid.SetTexture(renderer.CreateTexture(*texture, L"Grid/" + path.filename().wstring()));
        LogInfo("Dropped {}: grid texture, loaded in {:.0f} ms", name, timer.ElapsedMs());
    }
} // namespace sandbox
