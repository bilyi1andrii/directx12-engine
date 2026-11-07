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

#include "SandboxOptions.h"
#include "assets/AssetCache.h"
#include "render/Renderer.h"
#include "scene/Scene.h"

// Renders a square grid of cubes with models in a row, or a spinning cube if no model is loaded.
namespace sandbox {
    inline constexpr float kGridSpacing = 3.0f; // 2 units wide + 1 gap
    inline constexpr float kGridHeight = -4.0f;
    inline constexpr float kGridTop = kGridHeight + 1.0f;
    inline constexpr float kModelSize = 8.0f;
    inline constexpr float kModelSpacing = 10.0f;

    // Returns grid side length (smallest n where n * n >= count).
    [[nodiscard]] uint32_t GridSide(uint32_t count);

    // The grid's texture without --texture, relative to the executable (from the repository's data/ folder)
    inline constexpr std::wstring_view kDefaultGridTexture = L"data/textures/Endstone.jpg";

    // Loads the grid texture: the --texture file, else kDefaultGridTexture. If it fails, a warning and grey.
    [[nodiscard]] engine::render::TextureHandle CreateGridTexture(engine::render::Renderer &renderer,
                                                                  const SandboxOptions &options);

    // Creates a UV gradient texture for the spinning cube.
    [[nodiscard]] engine::render::TextureHandle CreateUvGradientTexture(engine::render::Renderer &renderer);

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
                         engine::scene::Scene &scene, float &nextModelX, engine::render::BatchKey &gridKey);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSCENE_H