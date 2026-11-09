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

    // Adds `count` cubes (rounded up to a square): movable in dynamic mode, fixed in static mode
    [[nodiscard]] GridObjects AddGrid(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                                      engine::render::TextureHandle texture, engine::apps::GridMode mode,
                                      uint32_t count);

    // Textures relative to the exe: the grid's without --texture, and the spinning cube's
    inline constexpr std::wstring_view kDefaultGridTexture = L"data/textures/Endstone.jpg";
    inline constexpr std::wstring_view kSpinningCubeTexture = L"data/textures/diamante.jpg";

    // Every model file in the gitignored assets/ folder, sorted by path
    [[nodiscard]] std::vector<std::filesystem::path> FindAssetModels();

    // Adds the model at `path`, standing on the grid's top at (x, z), with its largest side scaled to kModelSize.
    // If it cannot be loaded: a warning, and false.
    bool AddModel(engine::scene::Scene &scene, engine::assets::AssetCache &assets, const std::filesystem::path &path,
                  float x, float z);

    // Adds the models in a row along X, centred on the origin. Returns how many could be loaded.
    size_t AddModels(engine::scene::Scene &scene, engine::assets::AssetCache &assets,
                     std::span<const std::filesystem::path> paths);

    // A dropped model stands at `dropPoint`, or at `nextModelX` at the end of the row if there is no drop point.
    // Any other file is tried as the grid's texture (dynamic grid only).
    void LoadDroppedFile(const std::filesystem::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint,
                         engine::render::Renderer &renderer, engine::assets::AssetCache &assets,
                         engine::scene::Scene &scene, float &nextModelX, const GridObjects &grid);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSCENE_H