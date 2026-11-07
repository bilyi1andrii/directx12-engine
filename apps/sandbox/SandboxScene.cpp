#include "SandboxScene.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
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
    uint32_t GridSide(uint32_t count) {
        uint32_t side = 0;
        while (uint64_t{side} * side < count) {
            ++side;
        }
        return side;
    }

    engine::render::TextureHandle CreateGridTexture(engine::render::Renderer &renderer, const SandboxOptions &options) {
        // data/ is copied next to the executable by the build (data/CMakeLists.txt)
        const std::filesystem::path path =
            options.texturePath.value_or(engine::core::ExecutableDirectory() / kDefaultGridTexture);
        const std::expected<engine::render::TextureData, engine::core::Error> texture =
            engine::assets::LoadTexture(path, engine::assets::TextureUsage::Diffuse);
        if (!texture) {
            LogWarning("Grid texture: {}. Using the grey fallback.", texture.error().message);
            return renderer.UnloadedTexture();
        }
        return renderer.CreateTexture(*texture, L"Grid/" + path.filename().wstring());
    }

    engine::render::TextureHandle CreateUvGradientTexture(engine::render::Renderer &renderer) {
        constexpr uint32_t kSize = 256;
        std::vector<std::byte> rgba8(size_t{kSize} * kSize * 4);
        for (uint32_t y = 0; y < kSize; ++y) {
            for (uint32_t x = 0; x < kSize; ++x) {
                std::byte *pixel = &rgba8[(size_t{y} * kSize + x) * 4];
                pixel[0] = static_cast<std::byte>(x);
                pixel[1] = static_cast<std::byte>(y);
                pixel[2] = std::byte{64};
                pixel[3] = std::byte{255};
            }
        }
        const auto texture =
            engine::assets::CreateTextureFromPixels(rgba8, kSize, kSize, engine::assets::TextureUsage::Diffuse);
        if (!texture) {
            LogWarning("UV gradient texture: {}. Using the grey fallback.", texture.error().message);
            return renderer.UnloadedTexture();
        }
        return renderer.CreateTexture(*texture, L"SpinningCube/UvGradient");
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
        // Row vectors: centre on the origin with the bottom at 0, scale, then move onto the grid's top at (x, z)
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixTranslation(-(bounds.min.x + bounds.max.x) / 2, -bounds.min.y,
                                                    -(bounds.min.z + bounds.max.z) / 2) *
                                    XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, kGridTop, z));
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
                         engine::scene::Scene &scene, float &nextModelX, engine::render::BatchKey &gridKey) {
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
        const std::expected<engine::render::TextureData, engine::core::Error> texture =
            engine::assets::LoadTexture(path, engine::assets::TextureUsage::Diffuse);
        if (!texture) {
            LogWarning("Dropped {}: neither a model (.gltf, .glb, .obj, .fbx) nor a readable image: {}", name,
                       texture.error().message);
            return;
        }
        gridKey.material = {.index = renderer.CreateTexture(*texture, L"Grid/" + path.filename().wstring()).index};
        LogInfo("Dropped {}: grid texture, loaded in {:.0f} ms", name, timer.ElapsedMs());
    }
} // namespace sandbox
