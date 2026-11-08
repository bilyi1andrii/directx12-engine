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
#include "common/StressScene.h"
#include "render/Renderer.h"
#include "scene/Scene.h"

// The sandbox's scene objects: a cube grid laid out like the bench's, models in a row, or a spinning cube if there are
// no models.
namespace sandbox {
    inline constexpr float kModelSize = 8.0f;
    inline constexpr float kModelSpacing = 10.0f;

    // The grid's cubes in the scene: side * side objects, one after another from firstObject
    struct GridObjects {
        engine::apps::GridMode mode = engine::apps::GridMode::Dynamic;
        uint32_t side = 0;
        uint32_t firstObject = 0;
    };

    // A part that draws the cube mesh with `texture`
    [[nodiscard]] engine::render::ModelPart CubePart(engine::render::MeshHandle cube,
                                                     engine::render::TextureHandle texture);

    // Adds `count` cubes (rounded up to a square): movable in dynamic mode, fixed in static mode
    [[nodiscard]] GridObjects AddGrid(engine::scene::Scene &scene, engine::render::MeshHandle cube,
                                      engine::render::TextureHandle texture, engine::apps::GridMode mode,
                                      uint32_t count);

    // Textures relative to the exe: the grid's without --texture, and the spinning cube's
    inline constexpr std::wstring_view kDefaultGridTexture = L"data/textures/Endstone.jpg";
    inline constexpr std::wstring_view kSpinningCubeTexture = L"data/textures/diamante.jpg";

    // Loads kSpinningCubeTexture; grey with a warning on failure.
    [[nodiscard]] engine::render::TextureHandle CreateSpinningCubeTexture(engine::render::Renderer &renderer);

    // Finds all model files in assets/ sorted by path.
    [[nodiscard]] std::vector<std::filesystem::path> FindAssetModels();

    // Places a model centered at (x, z) on the grid top, scaled to kModelSize.
    bool AddModel(engine::scene::Scene &scene, engine::assets::AssetCache &assets, const std::filesystem::path &path,
                  float x, float z);

    // Places multiple models in a row along X. Returns loaded count.
    size_t AddModels(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                     std::span<const std::filesystem::path> paths);

    // Handles dropped files (spawns models at ray-grid intersection or row, or updates grid texture).
    void LoadDroppedFile(const std::filesystem::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint,
                         engine::render::Renderer &renderer, engine::assets::AssetCache &assets,
                         engine::scene::Scene &scene, float &nextModelX, const GridObjects &grid);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSCENE_H