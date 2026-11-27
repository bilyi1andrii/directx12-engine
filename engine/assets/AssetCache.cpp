#include "assets/AssetCache.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <DirectXMath.h>

#include "assets/ModelImporter.h"
#include "assets/Paths.h"
#include "assets/Primitives.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "core/Utf8.h"

namespace assets {
    namespace {
        using render::MaterialHandle;
        using render::TextureHandle;

        [[nodiscard]] uint8_t ToByte(float value) noexcept {
            return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        [[nodiscard]] std::wstring_view UsageName(TextureUsage usage) noexcept {
            switch (usage) {
            case TextureUsage::Diffuse:
                return L"colour";
            case TextureUsage::Opacity:
                return L"opacity";
            default:
                return IsSrgbUsage(usage) ? L"colour" : L"data";
            }
        }

        [[nodiscard]] render::Aabb Bounds(const render::ModelData &model) {
            using namespace DirectX;

            XMVECTOR low = XMVectorReplicate(FLT_MAX);
            XMVECTOR high = XMVectorReplicate(-FLT_MAX);
            const render::MeshData &mesh = model.mesh;
            for (const render::MeshNode &node : model.nodes) {
                const XMMATRIX transform = XMLoadFloat4x4(&node.transform);
                const render::SubmeshData &submesh = mesh.submeshes[node.submesh];
                for (uint32_t i = submesh.firstIndex; i < submesh.firstIndex + submesh.indexCount; ++i) {
                    const render::Vertex &vertex = mesh.vertices[submesh.baseVertex + mesh.indices[i]];
                    const XMVECTOR position = XMVector3TransformCoord(XMLoadFloat3(&vertex.position), transform);
                    low = XMVectorMin(low, position);
                    high = XMVectorMax(high, position);
                }
            }
            render::Aabb bounds;
            XMStoreFloat3(&bounds.min, low);
            XMStoreFloat3(&bounds.max, high);
            return bounds;
        }

    } // namespace

    std::expected<const render::Model *, core::Error> AssetCache::LoadModel(const std::filesystem::path &path) {
        const std::wstring key = NormalizedPath(path).native();
        if (const auto it = m_models.find(key); it != m_models.end()) {
            return &it->second;
        }

        core::Timer timer;
        const std::expected<render::ModelData, core::Error> data = ImportModel(path);
        if (!data) {
            return std::unexpected(data.error()); // not cached, fixed file loads next time
        }

        const std::wstring name = (path.parent_path().filename() / path.stem()).generic_wstring();
        const std::vector<render::MeshHandle> meshes = m_gpu->CreateMesh(data->mesh, L"Model/" + name);

        // each material made once per model
        std::vector<std::optional<MaterialHandle>> materials(data->materials.size());
        EmbeddedTextures embedded;
        const size_t texturesBefore = TextureCount();

        render::Model model;
        model.bounds = Bounds(*data);
        model.parts.reserve(data->nodes.size());
        for (const render::MeshNode &node : data->nodes) {
            const render::SubmeshData &submesh = data->mesh.submeshes[node.submesh];
            if (submesh.indexCount == 0) { // nothing to draw
                continue;
            }
            std::optional<MaterialHandle> &material = materials[submesh.materialIndex];
            if (!material) {
                material = MaterialFromFile(*data, data->materials[submesh.materialIndex], embedded, name);
            }
            model.parts.push_back(render::ModelPart{.mesh = meshes[node.submesh],
                                                    .material = *material,
                                                    .transform = node.transform,
                                                    .bounds = render::SubmeshBounds(data->mesh, submesh)});
            model.triangleCount += submesh.indexCount / 3;
        }

        core::LogInfo("AssetCache: {} in {:.0f} ms: {} parts, {} new textures", core::ToUtf8(name), timer.ElapsedMs(),
                      model.parts.size(), TextureCount() - texturesBefore);
        return &m_models.emplace(key, std::move(model)).first->second;
    }

    const render::Model &AssetCache::CubeModel(TextureHandle texture) {
        if (const auto it = m_cubeModels.find(texture.index); it != m_cubeModels.end()) {
            return it->second;
        }
        if (!m_cubeMesh) {
            m_cubeMesh = m_gpu->CreateMesh(MakeCube(), L"Cube").front();
        }
        render::Model model;
        model.parts.push_back(
            render::ModelPart{.mesh = *m_cubeMesh, .material = Material({.albedo = texture}), .bounds = kCubeBounds});
        model.bounds = kCubeBounds;
        model.triangleCount = kCubeTriangleCount;
        return m_cubeModels.emplace(texture.index, std::move(model)).first->second;
    }

    const render::Model &AssetCache::SphereModel(MaterialHandle material) {
        if (const auto it = m_sphereModels.find(material.index); it != m_sphereModels.end()) {
            return it->second;
        }
        if (!m_sphereMesh) {
            m_sphereMesh = m_gpu->CreateMesh(MakeSphere(), L"Sphere").front();
        }
        render::Model model;
        model.parts.push_back(render::ModelPart{.mesh = *m_sphereMesh, .material = material, .bounds = kSphereBounds});
        model.bounds = kSphereBounds;
        model.triangleCount = kSphereTriangleCount;
        return m_sphereModels.emplace(material.index, std::move(model)).first->second;
    }

    TextureHandle AssetCache::LoadTexture(const std::filesystem::path &path, TextureUsage usage) {
        const std::wstring key = std::format(L"{}|{}", NormalizedPath(path).native(), static_cast<int>(usage));
        if (const auto it = m_textures.find(key); it != m_textures.end()) {
            return it->second;
        }

        TextureHandle handle;
        if (const std::expected<render::TextureData, core::Error> data = ImportTexture(path, usage)) {
            handle = CreateTexture(*data, path.filename().native());
        } else {
            core::LogWarning("AssetCache: {}. Using the grey texture.", data.error().message);
            handle = m_gpu->UnloadedTexture();
        }
        m_textures.emplace(key, handle);
        return handle;
    }

    MaterialHandle AssetCache::MaterialFromFile(const render::ModelData &model, const render::MaterialDesc &material,
                                                EmbeddedTextures &embedded, std::wstring_view modelName) {
        const std::optional<TextureHandle> albedo =
            MaterialTexture(model, material.albedo, TextureUsage::Diffuse, embedded, modelName);
        render::MaterialInfo info{
            .albedo = albedo ? *albedo : SolidColor(material),
            .shading = render::Shading::Lit,
            .doubleSided = material.doubleSided,
            .alphaMode = material.alphaMode,
        };
        if (material.alphaMode != render::AlphaMode::Opaque) {
            info.alphaCutoff = ToByte(material.alphaCutoff);
            info.opacity = ToByte(material.baseColor.w);
            std::optional<TextureHandle> mask =
                MaterialTexture(model, material.opacityMap, TextureUsage::Opacity, embedded, modelName);
            // grey would cut out almost everything, use white
            if (mask == m_gpu->UnloadedTexture()) {
                core::LogWarning("AssetCache: material \"{}\" of {}: its opacity map did not load. Drawing it "
                                 "without one.",
                                 material.name, core::ToUtf8(modelName));
                mask.reset();
            }
            info.opacityMap = mask ? *mask : SolidColor(255, 255, 255);
        }
        return Material(info);
    }

    std::optional<TextureHandle> AssetCache::MaterialTexture(const render::ModelData &model,
                                                             const render::TextureSource &source, TextureUsage usage,
                                                             EmbeddedTextures &embedded, std::wstring_view modelName) {
        if (const auto *file = std::get_if<std::filesystem::path>(&source)) {
            return LoadTexture(*file, usage);
        }
        const auto *inFile = std::get_if<render::EmbeddedTexture>(&source);
        if (inFile == nullptr) {
            return std::nullopt;
        }
        const auto [it, added] = embedded.try_emplace({inFile->index, usage});
        if (added) {
            const render::EmbeddedTextureData &image = model.embeddedTextures[inFile->index];
            if (const auto data = ImportTextureFromMemory(image.bytes, image.formatHint, usage)) {
                it->second = CreateTexture(
                    *data, std::format(L"{}/Embedded {} ({})", modelName, inFile->index, UsageName(usage)));
            } else {
                core::LogWarning("AssetCache: {} of {}. Using the grey texture.", data.error().message,
                                 core::ToUtf8(modelName));
                it->second = m_gpu->UnloadedTexture();
            }
        }
        return it->second;
    }

    MaterialHandle AssetCache::Material(const render::MaterialInfo &material) {
        render::MaterialInfo used = material;
        const render::MaterialInfo defaults;
        // reset unused fields, so equal-looking materials share handle
        if (used.alphaMode != render::AlphaMode::Mask) {
            used.alphaCutoff = defaults.alphaCutoff;
        }
        if (used.alphaMode != render::AlphaMode::Blend) {
            used.opacity = defaults.opacity;
        }

        if (const auto it = m_materials.find(used); it != m_materials.end()) {
            return it->second;
        }
        const MaterialHandle handle = m_gpu->CreateMaterial(used);
        m_materials.emplace(used, handle);
        return handle;
    }

    size_t AssetCache::MaterialInfoHash::operator()(const render::MaterialInfo &material) const noexcept {
        const std::array<uint32_t, 4> words{
            material.albedo.index,
            material.opacityMap.index,
            uint32_t{std::to_underlying(material.shading)} | (uint32_t{material.doubleSided} << 8) |
                (uint32_t{std::to_underlying(material.alphaMode)} << 16) | (uint32_t{material.alphaCutoff} << 24),
            material.opacity,
        };
        return std::hash<std::string_view>{}(
            std::string_view(reinterpret_cast<const char *>(words.data()), sizeof(words)));
    }

    TextureHandle AssetCache::SolidColor(const render::MaterialDesc &material) {
        const uint8_t red = ToByte(material.baseColor.x);
        const uint8_t green = ToByte(material.baseColor.y);
        const uint8_t blue = ToByte(material.baseColor.z);
        if (red == 0 && green == 0 && blue == 0) { // files often say black for no colour
            return m_gpu->UnloadedTexture();
        }
        return SolidColor(red, green, blue);
    }

    TextureHandle AssetCache::SolidColor(uint8_t red, uint8_t green, uint8_t blue) {
        const uint32_t key = red | (green << 8) | (blue << 16);
        if (const auto it = m_solidColors.find(key); it != m_solidColors.end()) {
            return it->second;
        }
        const std::array pixel{std::byte{red}, std::byte{green}, std::byte{blue}, std::byte{255}};
        const TextureHandle handle = CreateTexture(MakeTextureFromPixels(pixel, 1, 1, TextureUsage::Diffuse).value(),
                                                   std::format(L"Color/{:02X}{:02X}{:02X}", red, green, blue));
        m_solidColors.emplace(key, handle);
        return handle;
    }

    TextureHandle AssetCache::SolidColor(const DirectX::XMFLOAT3 &color) {
        return SolidColor(ToByte(color.x), ToByte(color.y), ToByte(color.z));
    }

    TextureHandle AssetCache::CheckerboardTexture() {
        if (!m_checkerboard) {
            m_checkerboard = CreateTexture(MakeCheckerboard().value(), L"Checkerboard");
        }
        return *m_checkerboard;
    }

    TextureHandle AssetCache::CreateTexture(const render::TextureData &data, std::wstring_view name) {
        const TextureHandle handle = m_gpu->CreateTexture(data, name);
        ++m_createdTextures;
        return handle;
    }
} // namespace assets
