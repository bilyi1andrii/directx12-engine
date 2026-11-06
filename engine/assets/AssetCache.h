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
    // Loads models and textures from files once and keeps their GPU handles: the second request for the same
    // file, however it is spelled, returns what the first one created.
    //
    // Until materials get their own GPU data, a material is its albedo texture. A material without one
    // gets a 1x1 texture of its base colour (black becomes grey); a texture that fails to load becomes the
    // renderer's grey texture, with a warning.
    //
    // Needs a core::ComScope on this thread (image decoding). Not thread-safe. Nothing is ever unloaded: what
    // the cache created lives as long as the renderer, which must outlive the cache.
    class AssetCache {
    public:
        explicit AssetCache(render::GpuUploader &gpu) : m_gpu(&gpu) {}

        AssetCache(const AssetCache &) = delete;
        AssetCache &operator=(const AssetCache &) = delete;

        // The model in `path`, imported and uploaded on the first call. The pointer stays valid as long as the
        // cache. A file that fails to import is not remembered: the next call tries again.
        [[nodiscard]] std::expected<const render::Model *, core::Error> LoadModel(const std::filesystem::path &path);

        // The texture in `path` read as `usage`, loaded on the first call. A file that fails gives the grey
        // texture (also remembered, so it is not retried every time).
        [[nodiscard]] render::TextureHandle LoadTexture(const std::filesystem::path &path, TextureUsage usage);

        [[nodiscard]] size_t ModelCount() const noexcept { return m_models.size(); }
        // Distinct textures the cache asked the GPU for: files and solid colours
        [[nodiscard]] size_t TextureCount() const noexcept { return m_textures.size() + m_solidColors.size(); }

    private:
        // The material handle of `material` (its albedo texture). `model` holds its embedded images; `embedded`
        // remembers the ones already decoded for this model.
        [[nodiscard]] render::MaterialHandle Material(const render::ModelData &model,
                                                      const render::MaterialDesc &material,
                                                      std::vector<std::optional<render::TextureHandle>> &embedded,
                                                      std::wstring_view modelName);

        // A 1x1 sRGB texture of `material`'s base colour, made once per colour
        [[nodiscard]] render::TextureHandle SolidColor(const render::MaterialDesc &material);

        render::GpuUploader *m_gpu;
        // By normalized path. unordered_map never moves its elements, so pointers to the models stay valid.
        std::unordered_map<std::wstring, render::Model> m_models;
        // By normalized path plus the usage (one file read as colour and as data gives two textures)
        std::unordered_map<std::wstring, render::TextureHandle> m_textures;
        // By RGBA8 value
        std::unordered_map<uint32_t, render::TextureHandle> m_solidColors;
    };
} // namespace engine::assets

#endif // D3D12_ENGINE_ASSETCACHE_H
