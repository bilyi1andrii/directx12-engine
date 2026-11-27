#ifndef D3D12_ENGINE_ASSETCACHE_H
#define D3D12_ENGINE_ASSETCACHE_H

// Loads each model and texture file once and keeps what the renderer made of it. It turns a model's materials into
// the renderer's materials, and a file that fails to load becomes grey, with a warning.

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

namespace assets {
    class AssetCache {
    public:
        explicit AssetCache(render::GpuUploader &gpu) : m_gpu(&gpu) {}

        AssetCache(const AssetCache &) = delete;
        AssetCache &operator=(const AssetCache &) = delete;
        AssetCache(AssetCache &&) = delete;
        AssetCache &operator=(AssetCache &&) = delete;

        [[nodiscard]] std::expected<const render::Model *, core::Error> LoadModel(const std::filesystem::path &path);

        [[nodiscard]] const render::Model &CubeModel(render::TextureHandle texture);

        [[nodiscard]] const render::Model &SphereModel(render::MaterialHandle material);

        [[nodiscard]] render::TextureHandle LoadTexture(const std::filesystem::path &path, TextureUsage usage);

        [[nodiscard]] render::TextureHandle SolidColor(uint8_t red, uint8_t green, uint8_t blue);
        [[nodiscard]] render::TextureHandle SolidColor(const DirectX::XMFLOAT3 &color);

        [[nodiscard]] render::TextureHandle CheckerboardTexture();

        [[nodiscard]] render::MaterialHandle Material(const render::MaterialInfo &material);

        [[nodiscard]] size_t TextureCount() const noexcept { return m_createdTextures; }

    private:
        // per model load, image index and usage
        using EmbeddedTextures = std::map<std::pair<uint32_t, TextureUsage>, render::TextureHandle>;

        struct MaterialInfoHash {
            [[nodiscard]] size_t operator()(const render::MaterialInfo &material) const noexcept;
        };

        [[nodiscard]] render::MaterialHandle MaterialFromFile(const render::ModelData &model,
                                                              const render::MaterialDesc &material,
                                                              EmbeddedTextures &embedded, std::wstring_view modelName);

        [[nodiscard]] std::optional<render::TextureHandle>
        MaterialTexture(const render::ModelData &model, const render::TextureSource &source, TextureUsage usage,
                        EmbeddedTextures &embedded, std::wstring_view modelName);

        [[nodiscard]] render::TextureHandle SolidColor(const render::MaterialDesc &material);

        [[nodiscard]] render::TextureHandle CreateTexture(const render::TextureData &data, std::wstring_view name);

        render::GpuUploader *m_gpu;
        std::unordered_map<std::wstring, render::Model> m_models;
        std::optional<render::MeshHandle> m_cubeMesh;
        std::unordered_map<uint32_t, render::Model> m_cubeModels;
        std::optional<render::MeshHandle> m_sphereMesh;
        std::unordered_map<uint32_t, render::Model> m_sphereModels;
        // path and usage, one file can be colour and data
        std::unordered_map<std::wstring, render::TextureHandle> m_textures;
        std::unordered_map<uint32_t, render::TextureHandle> m_solidColors;
        std::unordered_map<render::MaterialInfo, render::MaterialHandle, MaterialInfoHash> m_materials;
        std::optional<render::TextureHandle> m_checkerboard;
        size_t m_createdTextures = 0;
    };
} // namespace assets

#endif // D3D12_ENGINE_ASSETCACHE_H
