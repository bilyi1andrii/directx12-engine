#ifndef D3D12_ENGINE_ASSETCACHE_H
#define D3D12_ENGINE_ASSETCACHE_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <DirectXMath.h>

#include "assets/TextureLoader.h"
#include "core/Error.h"
#include "renderdata/GpuUploader.h"
#include "renderdata/Handles.h"
#include "renderdata/Material.h"
#include "renderdata/MaterialDesc.h"
#include "renderdata/Model.h"
#include "renderdata/ModelData.h"

namespace engine::assets {
    // Loads each model and texture file once and keeps the GPU handles.
    // From a model's materials it uses the albedo texture, two-sidedness, and how the surface uses alpha (cut-out
    // or transparent, with its cutoff and mask). A material without a texture gets a 1x1 texture of its base colour;
    // black becomes grey. A texture that fails to load becomes grey, with a warning. A mask that fails to load is
    // left out, with a warning.
    // Needs a core::ComScope on this thread. Not thread-safe. Nothing is unloaded; the renderer must outlive it.
    class AssetCache {
    public:
        explicit AssetCache(render::GpuUploader &gpu) : m_gpu(&gpu) {}

        AssetCache(const AssetCache &) = delete;
        AssetCache &operator=(const AssetCache &) = delete;
        AssetCache(AssetCache &&) = delete;
        AssetCache &operator=(AssetCache &&) = delete;

        // Imports and uploads `path` on the first call; the pointer lives as long as the cache. Failures are not
        // cached: the next call tries again.
        [[nodiscard]] std::expected<const render::Model *, core::Error> LoadModel(const std::filesystem::path &path);

        // A model of one cube with `texture`. All cubes share one mesh, uploaded on the first call. The reference
        // lives as long as the cache.
        [[nodiscard]] const render::Model &CubeModel(render::TextureHandle texture);

        // A model of one sphere of radius 1 with `material`. All spheres share one mesh, uploaded on the first
        // call. The reference lives as long as the cache.
        [[nodiscard]] const render::Model &SphereModel(render::MaterialHandle material);

        // Loads `path` as `usage` on the first call. A failure gives the grey texture and is cached too.
        [[nodiscard]] render::TextureHandle LoadTexture(const std::filesystem::path &path, TextureUsage usage);

        // A 1x1 sRGB texture of one colour (values as in an image file), made once per colour
        [[nodiscard]] render::TextureHandle SolidColor(uint8_t red, uint8_t green, uint8_t blue);
        // The same, from a colour with channels from 0 to 1 (values outside are clamped)
        [[nodiscard]] render::TextureHandle SolidColor(const DirectX::XMFLOAT3 &color);

        // The light and dark grey checkerboard, made on the first call. A failure gives the grey texture.
        [[nodiscard]] render::TextureHandle CheckerboardTexture();

        // The material with these settings. Each distinct combination is created once, then reused. The alpha value
        // its mode does not use is reset to its default.
        [[nodiscard]] render::MaterialHandle Material(const render::MaterialInfo &material);

        [[nodiscard]] size_t ModelCount() const noexcept { return m_models.size(); }
        // Textures the cache created: files, embedded images and solid colours
        [[nodiscard]] size_t TextureCount() const noexcept { return m_createdTextures; }

    private:
        // A model's embedded images decoded so far. One image read as colour and as data is two textures.
        using EmbeddedTextures = std::map<std::pair<uint32_t, TextureUsage>, render::TextureHandle>;

        struct MaterialInfoHash {
            [[nodiscard]] size_t operator()(const render::MaterialInfo &material) const noexcept;
        };

        // The handle for a model's material
        [[nodiscard]] render::MaterialHandle MaterialFromFile(const render::ModelData &model,
                                                              const render::MaterialDesc &material,
                                                              EmbeddedTextures &embedded, std::wstring_view modelName);

        // The texture a material slot points to, read as the given kind of data, or nothing if the slot is empty. A
        // file or image that fails to load gives the grey texture, with a warning.
        [[nodiscard]] std::optional<render::TextureHandle>
        MaterialTexture(const render::ModelData &model, const render::TextureSource &source, TextureUsage usage,
                        EmbeddedTextures &embedded, std::wstring_view modelName);

        // A 1x1 sRGB texture of `material`'s base colour, made once per colour
        [[nodiscard]] render::TextureHandle SolidColor(const render::MaterialDesc &material);

        // Every texture the cache makes goes through here, so TextureCount() sees it
        [[nodiscard]] render::TextureHandle CreateTexture(const render::TextureData &data, std::wstring_view name);

        render::GpuUploader *m_gpu;
        // By normalized path; unordered_map keeps elements in place, so Model pointers stay valid
        std::unordered_map<std::wstring, render::Model> m_models;
        std::optional<render::MeshHandle> m_cubeMesh;
        // By texture index; unordered_map keeps elements in place here too
        std::unordered_map<uint32_t, render::Model> m_cubeModels;
        std::optional<render::MeshHandle> m_sphereMesh;
        // By material index
        std::unordered_map<uint32_t, render::Model> m_sphereModels;
        // By normalized path + usage (one file read as colour and as data is two textures)
        std::unordered_map<std::wstring, render::TextureHandle> m_textures;
        // By RGBA8 value
        std::unordered_map<uint32_t, render::TextureHandle> m_solidColors;
        std::unordered_map<render::MaterialInfo, render::MaterialHandle, MaterialInfoHash> m_materials;
        std::optional<render::TextureHandle> m_checkerboard;
        size_t m_createdTextures = 0;
    };
} // namespace engine::assets

#endif // D3D12_ENGINE_ASSETCACHE_H
