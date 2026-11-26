#ifndef D3D12_ENGINE_SANDBOXSCENE_H
#define D3D12_ENGINE_SANDBOXSCENE_H

// Builds the sandbox's scene: the cube grid, the models in assets/ and dropped files, 
// and the save of what the user moved.

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

namespace sandbox {
    inline constexpr float kModelSize = 8.0f;
    inline constexpr float kModelSpacing = 10.0f;

    struct GridObjects {
        apps::GridMode mode = apps::GridMode::Dynamic;
        uint32_t side = 0;
        uint32_t firstObject = 0;
    };

    [[nodiscard]] GridObjects AddGrid(scene::Scene &scene, assets::AssetCache &assets, render::TextureHandle texture,
                                      apps::GridMode mode, uint32_t count);

    [[nodiscard]] std::vector<DirectX::XMFLOAT4X4> GridPlacement(uint32_t side);

    inline constexpr std::wstring_view kDefaultGridTexture = L"data/textures/Endstone.jpg";
    inline constexpr std::wstring_view kSpinningCubeTexture = L"data/textures/diamante.jpg";

    [[nodiscard]] std::filesystem::path RepositoryRoot();

    [[nodiscard]] std::vector<std::filesystem::path> FindAssetModels();

    struct PlacedModel {
        scene::ObjectId id;
        std::filesystem::path path;
    };

    std::optional<PlacedModel> AddModel(scene::Scene &scene, assets::AssetCache &assets,
                                        const std::filesystem::path &path, float x, float z,
                                        const std::optional<DirectX::XMFLOAT4X4> &savedWorld = std::nullopt);

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
