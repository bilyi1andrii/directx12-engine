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

namespace fs = std::filesystem;

using core::LogInfo;
using core::LogWarning;

namespace sandbox {
    GridObjects AddGrid(scene::Scene &scene, assets::AssetCache &assets, render::TextureHandle texture,
                        apps::GridMode mode, uint32_t count) {
        using namespace DirectX;

        const render::Model &cube = assets.CubeModel(texture);
        const GridObjects grid{
            .mode = mode, .side = apps::GridSide(count), .firstObject = static_cast<uint32_t>(scene.Objects().size())};
        const scene::Mobility mobility =
            mode == apps::GridMode::Static ? scene::Mobility::Static : scene::Mobility::Movable;
        const std::vector<XMFLOAT4X4> placement = GridPlacement(grid.side);
        for (const XMFLOAT4X4 &world : placement) {
            scene.AddObject("Grid cube", cube, world, mobility);
        }
        LogInfo("Grid: {} x {} = {} cubes, {}", grid.side, grid.side, placement.size(), apps::ToString(mode));
        return grid;
    }

    std::vector<DirectX::XMFLOAT4X4> GridPlacement(uint32_t side) {
        using namespace DirectX;

        std::vector<XMFLOAT4X4> worlds(size_t{side} * side);
        for (uint32_t row = 0; row < side; ++row) {
            for (uint32_t col = 0; col < side; ++col) {
                const XMFLOAT3 offset =
                    render::GridCellOffset(row, col, side, side, apps::kGridSpacing, apps::kGridSpacing);
                XMStoreFloat4x4(&worlds[size_t{row} * side + col],
                                XMMatrixTranslation(offset.x, apps::kGridHeight, offset.z));
            }
        }
        return worlds;
    }

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

    std::optional<PlacedModel> AddModel(scene::Scene &scene, assets::AssetCache &assets, const fs::path &path, float x,
                                        float z, const std::optional<DirectX::XMFLOAT4X4> &savedWorld) {
        using namespace DirectX;

        const auto model = assets.LoadModel(path);
        if (!model) {
            LogWarning("Model: {}", model.error().message);
            return std::nullopt;
        }
        const render::Aabb &bounds = (*model)->bounds;
        const float extent =
            std::max({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z});
        const float scale = kModelSize / extent;
        const XMFLOAT3 pivot{(bounds.min.x + bounds.max.x) / 2, bounds.min.y, (bounds.min.z + bounds.max.z) / 2};
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, apps::kGridTop, z));
        std::string name = core::ToUtf8((path.parent_path().filename() / path.filename()).generic_wstring());
        LogInfo("Model {}: {} parts, largest side {:.2f} -> scaled by {:.4f}{}", name, (*model)->parts.size(), extent,
                scale, savedWorld ? " (placed as saved)" : "");
        const scene::ObjectId id =
            scene.AddObject(std::move(name), **model, savedWorld.value_or(world), scene::Mobility::Movable, pivot);
        return PlacedModel{.id = id, .path = path};
    }

    std::vector<PlacedModel> AddModels(scene::Scene &scene, assets::AssetCache &assets, std::span<const fs::path> paths,
                                       std::optional<float> firstX) {
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

    std::optional<float> NextFreeModelX(const scene::Scene &scene, std::span<const PlacedModel> models) {
        std::optional<float> next;
        for (const PlacedModel &model : models) {
            const float x = scene.ObjectPosition(model.id).x + kModelSpacing;
            next = next ? std::max(*next, x) : x;
        }
        return next;
    }

    void LoadDroppedFile(const fs::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint,
                         const render::GpuUploader &gpu, assets::AssetCache &assets, scene::Scene &scene,
                         float &nextModelX, const GridObjects &grid, std::vector<PlacedModel> &models) {
        core::Timer timer;
        const std::string name = core::ToUtf8(path.filename().wstring());
        LogInfo("Dropped {}: loading...", name);

        if (assets::IsModelFile(path)) {
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
        if (grid.mode != apps::GridMode::Dynamic) {
            LogWarning("Dropped {}: a static grid keeps its texture (start with --grid dynamic)", name);
            return;
        }

        const render::TextureHandle texture = assets.LoadTexture(path, assets::TextureUsage::Diffuse);
        if (texture == gpu.UnloadedTexture()) {
            LogWarning("Dropped {}: neither a model (.gltf, .glb, .obj, .fbx) nor a readable image", name);
            return;
        }
        const render::MaterialHandle material = assets.Material({.albedo = texture});
        for (uint32_t i = 0; i < grid.side * grid.side; ++i) {
            scene.SetObjectMaterial({.index = grid.firstObject + i}, material);
        }
        LogInfo("Dropped {}: grid texture, loaded in {:.0f} ms", name, timer.ElapsedMs());
    }

    fs::path SavePath() {
        return RepositoryRoot() / L"saves" / L"sandbox.json";
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

    SandboxSave MakeSave(const scene::Scene &scene, const GridObjects &grid, uint32_t gridCount,
                         std::span<const scene::ObjectId> namedCubes, std::span<const PlacedModel> models) {
        SandboxSave save{.gridCount = gridCount, .gridMode = std::string(apps::ToString(grid.mode))};
        if (grid.mode == apps::GridMode::Dynamic) {
            const std::vector<DirectX::XMFLOAT4X4> placement = GridPlacement(grid.side);
            for (uint32_t i = 0; i < placement.size(); ++i) {
                const DirectX::XMFLOAT4X4 &world = scene.Object({.index = grid.firstObject + i}).world;
                if (std::memcmp(&world, &placement[i], sizeof(world)) != 0) {
                    save.gridCubes.push_back({.index = i, .world = world});
                }
            }
        }
        for (const scene::ObjectId id : namedCubes) {
            const scene::SceneObject &cube = scene.Object(id);
            save.cubes.push_back({.name = cube.name, .world = cube.world});
        }
        for (const PlacedModel &model : models) {
            save.models.push_back({.path = model.path, .world = scene.Object(model.id).world});
        }
        return save;
    }
} // namespace sandbox
