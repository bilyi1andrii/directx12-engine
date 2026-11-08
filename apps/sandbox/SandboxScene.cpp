#include "SandboxScene.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "assets/ModelImporter.h"
#include "assets/Primitives.h"
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

    engine::render::ModelPart CubePart(engine::render::MeshHandle cube, engine::render::TextureHandle texture) {
        return {.mesh = cube, .material = {.index = texture.index}, .bounds = engine::assets::kCubeBounds};
    }

    GridObjects AddGrid(engine::scene::Scene &scene, engine::render::MeshHandle cube,
                        engine::render::TextureHandle texture, engine::apps::GridMode mode, uint32_t count) {
        using namespace DirectX;

        const engine::render::ModelPart part = CubePart(cube, texture);
        const GridObjects grid{.mode = mode,
                               .side = engine::apps::GridSide(count),
                               .firstObject = static_cast<uint32_t>(scene.Objects().size())};
        XMFLOAT4X4 base{};
        XMStoreFloat4x4(&base, XMMatrixTranslation(0.0f, engine::apps::kGridHeight, 0.0f));

        // SubmitGrid knows where each cube goes, so the layout stays the same as the bench's
        engine::render::RenderList placed;
        engine::render::SubmitGrid(placed, {.mesh = part.mesh, .material = part.material}, base, grid.side, grid.side,
                                   engine::apps::kGridSpacing, engine::apps::kGridSpacing);
        const engine::scene::Mobility mobility =
            mode == engine::apps::GridMode::Static ? engine::scene::Mobility::Static : engine::scene::Mobility::Movable;
        for (const engine::render::BatchBucket &bucket : placed.Buckets()) {
            for (const XMFLOAT4X4 &world : bucket.worlds) {
                scene.AddObject("Grid cube", {&part, 1}, world, mobility);
            }
        }
        LogInfo("Grid: {} x {} = {} cubes, {}", grid.side, grid.side, placed.InstanceCount(),
                engine::apps::ToString(mode));
        return grid;
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
        // The pivot is the bottom centre: world scales the model around it and stands it at (x, kGridTop, z)
        const XMFLOAT3 pivot{(bounds.min.x + bounds.max.x) / 2, bounds.min.y, (bounds.min.z + bounds.max.z) / 2};
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world,
                        XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, engine::apps::kGridTop, z));
        // Folder/file: model files are often all named scene.gltf
        std::string name = engine::core::ToUtf8((path.parent_path().filename() / path.filename()).generic_wstring());
        LogInfo("Model {}: {} parts, largest side {:.2f} -> scaled by {:.4f}", name, (*model)->parts.size(), extent,
                scale);
        scene.AddObject(std::move(name), (*model)->parts, world, engine::scene::Mobility::Movable, pivot);
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
                         engine::scene::Scene &scene, float &nextModelX, const GridObjects &grid) {
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
        if (grid.mode != engine::apps::GridMode::Dynamic) {
            LogWarning("Dropped {}: a static grid keeps its texture (start with --grid dynamic)", name);
            return;
        }

        const engine::render::TextureHandle texture = assets.LoadTexture(path, engine::assets::TextureUsage::Diffuse);
        if (texture == renderer.UnloadedTexture()) {
            LogWarning("Dropped {}: neither a model (.gltf, .glb, .obj, .fbx) nor a readable image", name);
            return;
        }
        // A material is its albedo texture for now (Handles.h)
        for (uint32_t i = 0; i < grid.side * grid.side; ++i) {
            scene.SetObjectMaterial({.index = grid.firstObject + i}, {.index = texture.index});
        }
        LogInfo("Dropped {}: grid texture, loaded in {:.0f} ms", name, timer.ElapsedMs());
    }
} // namespace sandbox
