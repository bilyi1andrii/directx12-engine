#ifndef D3D12_ENGINE_ASSETCACHE_H
#define D3D12_ENGINE_ASSETCACHE_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets/TextureLoader.h"
#include "core/Error.h"
#include "renderdata/GpuUploader.h"
#include "renderdata/Handles.h"
#include "renderdata/MaterialDesc.h"
#include "renderdata/Model.h"
#include "renderdata/ModelData.h"

namespace engine::assets {
    // Loads each model and texture file once and keeps the GPU handles.
    // A material is its albedo texture for now: without one, a 1x1 texture of its base colour (black -> grey);
    // a texture that fails to load is grey, with a warning.
    // Needs a core::ComScope on this thread. Not thread-safe. Nothing is unloaded; the renderer must outlive it.
    class AssetCache {
    public:
        explicit AssetCache(render::GpuUploader &gpu) : m_gpu(&gpu) {}

        AssetCache(const AssetCache &) = delete;
        AssetCache &operator=(const AssetCache &) = delete;

        // Imports and uploads `path` on the first call; the pointer lives as long as the cache. Failures are not
        // cached: the next call tries again.
        [[nodiscard]] std::expected<const render::Model *, core::Error> LoadModel(const std::filesystem::path &path);

        // Loads `path` as `usage` on the first call. A failure gives the grey texture and is cached too.
        [[nodiscard]] render::TextureHandle LoadTexture(const std::filesystem::path &path, TextureUsage usage);

        [[nodiscard]] size_t ModelCount() const noexcept { return m_models.size(); }
        // Textures created so far: files and solid colours
        [[nodiscard]] size_t TextureCount() const noexcept { return m_textures.size() + m_solidColors.size(); }

    private:
        // `material`'s handle (its albedo texture). `embedded` remembers the images of `model` already decoded.
        [[nodiscard]] render::MaterialHandle Material(const render::ModelData &model,
                                                      const render::MaterialDesc &material,
                                                      std::vector<std::optional<render::TextureHandle>> &embedded,
                                                      std::wstring_view modelName);

        // A 1x1 sRGB texture of `material`'s base colour, made once per colour
        [[nodiscard]] render::TextureHandle SolidColor(const render::MaterialDesc &material);

        render::GpuUploader *m_gpu;
        // By normalized path; unordered_map keeps elements in place, so Model pointers stay valid
        std::unordered_map<std::wstring, render::Model> m_models;
        // By normalized path + usage (one file read as colour and as data is two textures)
        std::unordered_map<std::wstring, render::TextureHandle> m_textures;
        // By RGBA8 value
        std::unordered_map<uint32_t, render::TextureHandle> m_solidColors;
    };
} // namespace engine::assets

#endif // D3D12_ENGINE_ASSETCACHE_H
