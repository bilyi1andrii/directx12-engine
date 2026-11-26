#ifndef D3D12_ENGINE_SANDBOXSCENE_H
#define D3D12_ENGINE_SANDBOXSCENE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <DirectXMath.h>

#include "assets/AssetCache.h"
#include "common/GridSetup.h"
#include "renderdata/GpuUploader.h"
#include "sandbox/SceneSave.h"
#include "scene/Scene.h"

// The sandbox's scene objects: a cube grid laid out like the bench's, models in a row, or a spinning cube if there are
// no models.
namespace sandbox {
    inline constexpr float kModelSize = 8.0f;
    inline constexpr float kModelSpacing = 10.0f;

    // The grid's cubes in the scene: side * side objects, one after another from firstObject
    struct GridObjects {
        apps::GridMode mode = apps::GridMode::Dynamic;
        uint32_t side = 0;
        uint32_t firstObject = 0;
    };

    // Adds `count` cubes (rounded up to a square): movable in dynamic mode, fixed in static mode
    [[nodiscard]] GridObjects AddGrid(scene::Scene &scene, assets::AssetCache &assets, render::TextureHandle texture,
                                      apps::GridMode mode, uint32_t count);

    [[nodiscard]] std::vector<DirectX::XMFLOAT4X4> GridPlacement(uint32_t side);

    // Textures relative to the exe: the grid's without --texture, and the spinning cube's
    inline constexpr std::wstring_view kDefaultGridTexture = L"data/textures/Endstone.jpg";
    inline constexpr std::wstring_view kSpinningCubeTexture = L"data/textures/diamante.jpg";

    [[nodiscard]] std::filesystem::path RepositoryRoot();

    // Every model file in the gitignored assets/ folder, sorted by path
    [[nodiscard]] std::vector<std::filesystem::path> FindAssetModels();

    // A model in the scene and the file it came from, so that it can be saved
    struct PlacedModel {
        scene::ObjectId id;
        std::filesystem::path path;
    };

    // Adds the model at `path`, standing on the grid's top at (x, z), with its largest side scaled to kModelSize.
    // If it cannot be loaded: a warning, and false.
    std::optional<PlacedModel> AddModel(scene::Scene &scene, assets::AssetCache &assets,
                                        const std::filesystem::path &path, float x, float z,
                                        const std::optional<DirectX::XMFLOAT4X4> &savedWorld = std::nullopt);

    // Adds the models in a row along X, centred on the origin. Returns how many could be loaded.
    std::vector<PlacedModel> AddModels(scene::Scene &scene, assets::AssetCache &assets,
                                       std::span<const std::filesystem::path> paths,
                                       std::optional<float> firstX = std::nullopt);

    void LoadDroppedFile(const std::filesystem::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint,
                         const render::GpuUploader &gpu, assets::AssetCache &assets, scene::Scene &scene,
                         float &nextModelX, const GridObjects &grid, std::vector<PlacedModel> &models);

    [[nodiscard]] std::filesystem::path SavePath();

    [[nodiscard]] std::optional<SandboxSave> ReadSave();

    [[nodiscard]] SandboxSave MakeSave(const scene::Scene &scene, const GridObjects &grid, uint32_t gridCount,
                                       std::span<const scene::ObjectId> namedCubes,
                                       std::span<const PlacedModel> models);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSCENE_H