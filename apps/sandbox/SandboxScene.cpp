#include "SandboxScene.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "assets/ModelImporter.h"
#include "assets/TextureLoader.h"
#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "core/Utf8.h"
#include "renderdata/GridLayout.h"

using engine::core::LogInfo;
using engine::core::LogWarning;

namespace sandbox {
    GridObjects AddGrid(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                        engine::render::TextureHandle texture, engine::apps::GridMode mode, uint32_t count) {
        using namespace DirectX;

        const engine::render::Model &cube = assets.CubeModel(texture);
        const GridObjects grid{.mode = mode,
                               .side = engine::apps::GridSide(count),
                               .firstObject = static_cast<uint32_t>(scene.Objects().size())};
        const engine::scene::Mobility mobility =
            mode == engine::apps::GridMode::Static ? engine::scene::Mobility::Static : engine::scene::Mobility::Movable;
        const std::vector<XMFLOAT4X4> placement = GridPlacement(grid.side);
        for (const XMFLOAT4X4 &world : placement) {
            scene.AddObject("Grid cube", cube, world, mobility);
        }
        LogInfo("Grid: {} x {} = {} cubes, {}", grid.side, grid.side, placement.size(), engine::apps::ToString(mode));
        return grid;
    }

    std::vector<DirectX::XMFLOAT4X4> GridPlacement(uint32_t side) {
        using namespace DirectX;

        std::vector<XMFLOAT4X4> worlds(size_t{side} * side);
        for (uint32_t row = 0; row < side; ++row) {
            for (uint32_t col = 0; col < side; ++col) {
                const XMFLOAT3 offset = engine::render::GridCellOffset(row, col, side, side, engine::apps::kGridSpacing,
                                                                       engine::apps::kGridSpacing);
                XMStoreFloat4x4(&worlds[size_t{row} * side + col],
                                XMMatrixTranslation(offset.x, engine::apps::kGridHeight, offset.z));
            }
        }
        return worlds;
    }

    std::filesystem::path RepositoryRoot() {
        return (engine::core::ExecutableDirectory() / L"../../..").lexically_normal();
    }

    std::vector<std::filesystem::path> FindAssetModels() {
        const std::filesystem::path root = RepositoryRoot() / L"assets";
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

    std::optional<PlacedModel> AddModel(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                                        const std::filesystem::path &path, float x, float z,
                                        const std::optional<DirectX::XMFLOAT4X4> &savedWorld) {
        using namespace DirectX;

        const auto model = assets.LoadModel(path);
        if (!model) {
            LogWarning("Model: {}", model.error().message);
            return std::nullopt;
        }
        const engine::render::Aabb &bounds = (*model)->bounds;
        const float extent =
            std::max({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z, 1e-6f});
        const float scale = kModelSize / extent;
        // The pivot is the model's bottom centre, so the model stands on the grid at (x, z)
        const XMFLOAT3 pivot{(bounds.min.x + bounds.max.x) / 2, bounds.min.y, (bounds.min.z + bounds.max.z) / 2};
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world,
                        XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, engine::apps::kGridTop, z));
        // Include the folder: model files are often all named scene.gltf
        std::string name = engine::core::ToUtf8((path.parent_path().filename() / path.filename()).generic_wstring());
        LogInfo("Model {}: {} parts, largest side {:.2f} -> scaled by {:.4f}{}", name, (*model)->parts.size(), extent,
                scale, savedWorld ? " (placed as saved)" : "");
        const engine::scene::ObjectId id = scene.AddObject(std::move(name), **model, savedWorld.value_or(world),
                                                           engine::scene::Mobility::Movable, pivot);
        return PlacedModel{.id = id, .path = path};
    }

    std::vector<PlacedModel> AddModels(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                                       std::span<const std::filesystem::path> paths, std::optional<float> firstX) {
        std::vector<PlacedModel> added;
        for (size_t i = 0; i < paths.size(); ++i) {
            const float x = firstX ? *firstX + static_cast<float>(i) * kModelSpacing
                                   : (static_cast<float>(i) - static_cast<float>(paths.size() - 1) / 2) * kModelSpacing;
            if (std::optional<PlacedModel> model = AddModel(scene, assets, paths[i], x, 0.0f)) {
                added.push_back(std::move(*model));
            }
        }
        return added;
    }

    void LoadDroppedFile(const std::filesystem::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint,
                         const engine::render::GpuUploader &gpu, engine::assets::AssetCache &assets,
                         engine::scene::Scene &scene, float &nextModelX, const GridObjects &grid,
                         std::vector<PlacedModel> &models) {
        engine::core::Timer timer;
        timer.Start();
        const std::string name = engine::core::ToUtf8(path.filename().wstring());
        LogInfo("Dropped {}: loading...", name);

        if (engine::assets::IsModelFile(path)) {
            const float x = dropPoint ? dropPoint->x : nextModelX;
            const float z = dropPoint ? dropPoint->z : 0.0f;
            if (std::optional<PlacedModel> model = AddModel(scene, assets, path, x, z)) {
                models.push_back(std::move(*model));
                if (!dropPoint) {
                    nextModelX += kModelSpacing;
                }
                LogInfo("Dropped {}: loaded in {:.0f} ms at ({:.1f}, {:.1f})", name, timer.ElapsedMs(), x, z);
            }
            return;
        }
        if (grid.mode != engine::apps::GridMode::Dynamic) {
            LogWarning("Dropped {}: a static grid keeps its texture (start with --grid dynamic)", name);
            return;
        }

        const engine::render::TextureHandle texture = assets.LoadTexture(path, engine::assets::TextureUsage::Diffuse);
        if (texture == gpu.UnloadedTexture()) {
            LogWarning("Dropped {}: neither a model (.gltf, .glb, .obj, .fbx) nor a readable image", name);
            return;
        }
        const engine::render::MaterialHandle material = assets.Material(texture);
        for (uint32_t i = 0; i < grid.side * grid.side; ++i) {
            scene.SetObjectMaterial({.index = grid.firstObject + i}, material);
        }
        LogInfo("Dropped {}: grid texture, loaded in {:.0f} ms", name, timer.ElapsedMs());
    }

    std::filesystem::path SavePath() {
        return RepositoryRoot() / L"saves" / L"sandbox.json";
    }

    std::optional<SandboxSave> ReadSave() {
        const std::filesystem::path file = SavePath();
        std::expected<SandboxSave, engine::core::Error> save = ReadSandboxSave(file, RepositoryRoot());
        if (save) {
            LogInfo("Scene: {} ({} models, {} cubes, {} grid cubes)", engine::core::ToUtf8(file.native()),
                    save->models.size(), save->cubes.size(), save->gridCubes.size());
            return std::move(*save);
        }
        if (save.error().code == engine::core::ErrorCode::FileNotFound) {
            return std::nullopt;
        }
        std::filesystem::path bad = file;
        bad += L".bad";
        std::error_code ec;
        std::filesystem::rename(file, bad, ec);
        LogWarning("Scene: {}. Starting without it; the file is kept as {}", save.error().message,
                   engine::core::ToUtf8(bad.filename().native()));
        return std::nullopt;
    }

    SandboxSave MakeSave(const engine::scene::Scene &scene, const GridObjects &grid, uint32_t gridCount,
                         std::span<const engine::scene::ObjectId> namedCubes, std::span<const PlacedModel> models) {
        SandboxSave save{.gridCount = gridCount, .gridMode = std::string(engine::apps::ToString(grid.mode))};
        // Only cubes the user changed. A static grid cannot change.
        if (grid.mode == engine::apps::GridMode::Dynamic) {
            const std::vector<DirectX::XMFLOAT4X4> placement = GridPlacement(grid.side);
            for (uint32_t i = 0; i < placement.size(); ++i) {
                const DirectX::XMFLOAT4X4 &world = scene.Object({.index = grid.firstObject + i}).world;
                if (std::memcmp(&world, &placement[i], sizeof(world)) != 0) {
                    save.gridCubes.push_back({.index = i, .world = world});
                }
            }
        }
        for (const engine::scene::ObjectId id : namedCubes) {
            const engine::scene::SceneObject &cube = scene.Object(id);
            save.cubes.push_back({.name = cube.name, .world = cube.world});
        }
        for (const PlacedModel &model : models) {
            save.models.push_back({.path = model.path, .world = scene.Object(model.id).world});
        }
        return save;
    }
} // namespace sandbox
