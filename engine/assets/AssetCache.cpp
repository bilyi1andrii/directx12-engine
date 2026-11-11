#include "assets/AssetCache.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <format>
#include <optional>
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

namespace engine::assets {
    namespace {
        using render::MaterialHandle;
        using render::TextureHandle;

        [[nodiscard]] uint8_t ToByte(float value) noexcept {
            return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        // The box around every placed submesh, in model space
        [[nodiscard]] render::Aabb Bounds(const render::ModelData &model) {
            using namespace DirectX;

            XMVECTOR low = XMVectorReplicate(FLT_MAX);
            XMVECTOR high = XMVectorReplicate(-FLT_MAX);
            bool any = false;
            const render::MeshData &mesh = model.mesh;
            for (const render::MeshNode &node : model.nodes) {
                const XMMATRIX transform = XMLoadFloat4x4(&node.transform);
                const render::SubmeshData &submesh = mesh.submeshes[node.submesh];
                for (uint32_t i = submesh.firstIndex; i < submesh.firstIndex + submesh.indexCount; ++i) {
                    const render::Vertex &vertex = mesh.vertices[submesh.baseVertex + mesh.indices[i]];
                    const XMVECTOR position = XMVector3TransformCoord(XMLoadFloat3(&vertex.position), transform);
                    low = XMVectorMin(low, position);
                    high = XMVectorMax(high, position);
                    any = true;
                }
            }
            render::Aabb bounds;
            if (any) {
                XMStoreFloat3(&bounds.min, low);
                XMStoreFloat3(&bounds.max, high);
            }
            return bounds;
        }

        // The box around one submesh, in its own space
        [[nodiscard]] render::Aabb SubmeshBounds(const render::MeshData &mesh, const render::SubmeshData &submesh) {
            using namespace DirectX;

            XMVECTOR low = XMVectorReplicate(FLT_MAX);
            XMVECTOR high = XMVectorReplicate(-FLT_MAX);
            for (uint32_t i = submesh.firstIndex; i < submesh.firstIndex + submesh.indexCount; ++i) {
                const XMVECTOR position = XMLoadFloat3(&mesh.vertices[submesh.baseVertex + mesh.indices[i]].position);
                low = XMVectorMin(low, position);
                high = XMVectorMax(high, position);
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
        timer.Start();
        const std::expected<render::ModelData, core::Error> data = ImportModel(path);
        if (!data) {
            return std::unexpected(data.error());
        }

        // Folder/stem: model files are often all named scene.gltf
        const std::wstring name = (path.parent_path().filename() / path.stem()).generic_wstring();
        const std::vector<render::MeshHandle> meshes = m_gpu->CreateMesh(data->mesh, L"Model/" + name);

        // Only the materials a part uses get textures; each embedded image is decoded once
        std::vector<std::optional<MaterialHandle>> materials(data->materials.size());
        std::vector<std::optional<TextureHandle>> embedded(data->embeddedTextures.size());
        const size_t texturesBefore = TextureCount();

        render::Model model;
        model.bounds = Bounds(*data);
        model.parts.reserve(data->nodes.size());
        for (const render::MeshNode &node : data->nodes) {
            const render::SubmeshData &submesh = data->mesh.submeshes[node.submesh];
            if (submesh.indexCount == 0) {
                continue; // only points or lines: nothing the renderer draws
            }
            std::optional<MaterialHandle> &material = materials[submesh.materialIndex];
            if (!material) {
                material = Material(*data, data->materials[submesh.materialIndex], embedded, name);
            }
            model.parts.push_back(render::ModelPart{.mesh = meshes[node.submesh],
                                                    .material = *material,
                                                    .transform = node.transform,
                                                    .bounds = SubmeshBounds(data->mesh, submesh)});
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
            render::ModelPart{.mesh = *m_cubeMesh, .material = Material(texture), .bounds = kCubeBounds});
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

    MaterialHandle AssetCache::Material(const render::ModelData &model, const render::MaterialDesc &material,
                                        std::vector<std::optional<TextureHandle>> &embedded,
                                        std::wstring_view modelName) {
        TextureHandle albedo;
        if (const auto *file = std::get_if<std::filesystem::path>(&material.albedo)) {
            albedo = LoadTexture(*file, TextureUsage::Diffuse);
        } else if (const auto *inFile = std::get_if<render::EmbeddedTexture>(&material.albedo)) {
            std::optional<TextureHandle> &texture = embedded[inFile->index];
            if (!texture) {
                const render::EmbeddedTextureData &image = model.embeddedTextures[inFile->index];
                if (const auto data = ImportTextureFromMemory(image.bytes, image.formatHint, TextureUsage::Diffuse)) {
                    texture = CreateTexture(*data, std::format(L"{}/Embedded {}", modelName, inFile->index));
                } else {
                    core::LogWarning("AssetCache: {} of {}. Using the grey texture.", data.error().message,
                                     core::ToUtf8(modelName));
                    texture = m_gpu->UnloadedTexture();
                }
            }
            albedo = *texture;
        } else {
            albedo = SolidColor(material);
        }
        return Material(albedo, render::Shading::Lit, material.doubleSided);
    }

    MaterialHandle AssetCache::Material(TextureHandle albedo, render::Shading shading, bool doubleSided) {
        const uint64_t key =
            albedo.index | (uint64_t{std::to_underlying(shading)} << 32) | (uint64_t{doubleSided} << 40);
        if (const auto it = m_materials.find(key); it != m_materials.end()) {
            return it->second;
        }
        const MaterialHandle handle =
            m_gpu->CreateMaterial({.albedo = albedo, .shading = shading, .doubleSided = doubleSided});
        m_materials.emplace(key, handle);
        return handle;
    }

    TextureHandle AssetCache::SolidColor(const render::MaterialDesc &material) {
        // The colour as is: Kd * 255 into an sRGB texture, no linear -> sRGB conversion
        const uint8_t red = ToByte(material.baseColor.x);
        const uint8_t green = ToByte(material.baseColor.y);
        const uint8_t blue = ToByte(material.baseColor.z);
        // A black material shows the grey unloaded texture
        if (red == 0 && green == 0 && blue == 0) {
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
        TextureHandle handle = m_gpu->UnloadedTexture();
        if (const auto data = MakeTextureFromPixels(pixel, 1, 1, TextureUsage::Diffuse)) {
            handle = CreateTexture(*data, std::format(L"Color/{:02X}{:02X}{:02X}", red, green, blue));
        }
        m_solidColors.emplace(key, handle);
        return handle;
    }

    TextureHandle AssetCache::CheckerboardTexture() {
        if (!m_checkerboard) {
            if (const std::expected<render::TextureData, core::Error> data = MakeCheckerboard()) {
                m_checkerboard = CreateTexture(*data, L"Checkerboard");
            } else {
                core::LogWarning("AssetCache: {}. Using the grey texture.", data.error().message);
                m_checkerboard = m_gpu->UnloadedTexture();
            }
        }
        return *m_checkerboard;
    }

    TextureHandle AssetCache::CreateTexture(const render::TextureData &data, std::wstring_view name) {
        const TextureHandle handle = m_gpu->CreateTexture(data, name);
        ++m_createdTextures;
        return handle;
    }
} // namespace engine::assets
