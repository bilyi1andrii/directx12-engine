#include "assets/ModelImporter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <DirectXMath.h>
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "assets/Paths.h"
#include "core/Log.h"
#include "core/Utf8.h"

namespace engine::assets {
    namespace {
        using core::Error;
        using core::ErrorCode;
        using core::LogWarning;
        using render::AlphaMode;
        using render::EmbeddedTexture;
        using render::MaterialDesc;
        using render::MeshData;
        using render::MeshNode;
        using render::ModelData;
        using render::SubmeshData;
        using render::TextureSource;
        using render::Vertex;

        constexpr unsigned kImportFlags = aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
                                          aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;

        // Appends `mesh` to the vertices and indices of `geometry` as one submesh. Points and lines are left out.
        void AddSubmesh(const aiMesh &mesh, std::string_view source, MeshData &geometry) {
            SubmeshData submesh{
                .firstIndex = static_cast<uint32_t>(geometry.indices.size()),
                .baseVertex = static_cast<int32_t>(geometry.vertices.size()),
                .materialIndex = mesh.mMaterialIndex,
            };

            geometry.vertices.reserve(geometry.vertices.size() + mesh.mNumVertices);
            for (unsigned i = 0; i < mesh.mNumVertices; ++i) {
                Vertex vertex{};
                vertex.position = {mesh.mVertices[i].x, mesh.mVertices[i].y, mesh.mVertices[i].z};
                if (mesh.HasTextureCoords(0)) {
                    vertex.uv = {mesh.mTextureCoords[0][i].x, mesh.mTextureCoords[0][i].y};
                }
                if (mesh.HasNormals()) {
                    vertex.normal = {mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z};
                }
                vertex.tangent = mesh.HasTangentsAndBitangents()
                                     ? DirectX::XMFLOAT3{mesh.mTangents[i].x, mesh.mTangents[i].y, mesh.mTangents[i].z}
                                     : DirectX::XMFLOAT3{1.0f, 0.0f, 0.0f};
                geometry.vertices.push_back(vertex);
            }

            unsigned skippedFaces = 0;
            geometry.indices.reserve(geometry.indices.size() + size_t{mesh.mNumFaces} * 3);
            for (unsigned i = 0; i < mesh.mNumFaces; ++i) {
                const aiFace &face = mesh.mFaces[i];
                if (face.mNumIndices != 3) {
                    ++skippedFaces;
                    continue;
                }
                geometry.indices.insert(geometry.indices.end(), {face.mIndices[0], face.mIndices[1], face.mIndices[2]});
            }
            if (skippedFaces > 0) {
                LogWarning("{}: mesh \"{}\": skipped {} faces that are points or lines", source, mesh.mName.C_Str(),
                           skippedFaces);
            }

            submesh.indexCount = static_cast<uint32_t>(geometry.indices.size()) - submesh.firstIndex;
            geometry.submeshes.push_back(submesh);
        }

        // Adds one MeshNode per mesh of `node` and of its children. Each transform already includes the parents'.
        void AddNodes(const aiNode &node, DirectX::FXMMATRIX parent, std::vector<MeshNode> &nodes) {
            // Transpose Assimp column-major matrix to engine's row-major space.
            const DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(
                DirectX::XMMatrixTranspose(DirectX::XMMATRIX(&node.mTransformation.a1)), parent);

            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                MeshNode meshNode{.submesh = node.mMeshes[i]};
                DirectX::XMStoreFloat4x4(&meshNode.transform, transform);
                nodes.push_back(meshNode);
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                AddNodes(*node.mChildren[i], transform, nodes);
            }
        }

        [[nodiscard]] std::optional<std::filesystem::path> PathFromUtf8(std::string_view text) {
            try {
                return std::filesystem::path(
                    std::u8string_view(reinterpret_cast<const char8_t *>(text.data()), text.size()));
            } catch (const std::system_error &) {
                return std::nullopt;
            }
        }

        [[nodiscard]] bool IsSet(const TextureSource &texture) noexcept {
            return !std::holds_alternative<std::monostate>(texture);
        }

        // Reads materials, and collects the embedded images they use into `model`
        struct MaterialReader {
            const aiScene &scene;
            std::filesystem::path directory;
            std::string_view source;
            ModelData &model;
            std::unordered_map<const aiTexture *, uint32_t> embeddedIndices{};

            [[nodiscard]] MaterialDesc Read(const aiMaterial &material) {
                MaterialDesc desc;
                aiString name;
                if (material.Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                    desc.name = name.C_Str();
                }

                ReadFactors(material, desc);

                desc.normal = FirstTexture(material, aiTextureType_NORMALS);
                desc.emissive = FirstTexture(material, aiTextureType_EMISSIVE);
                desc.height = FirstTexture(material, aiTextureType_HEIGHT);
                desc.opacityMap = FirstTexture(material, aiTextureType_OPACITY);
                desc.ambientOcclusion = FirstTexture(material, aiTextureType_AMBIENT_OCCLUSION);
                if (!IsSet(desc.ambientOcclusion)) {
                    desc.ambientOcclusion = FirstTexture(material, aiTextureType_LIGHTMAP);
                }

                desc.albedo = FirstTexture(material, aiTextureType_BASE_COLOR);
                if (TextureSource packed = FirstTexture(material, aiTextureType_GLTF_METALLIC_ROUGHNESS);
                    IsSet(packed)) {
                    desc.metallic = packed;
                    desc.roughness = std::move(packed);
                    desc.packedMetallicRoughness = true;
                } else {
                    desc.metallic = FirstTexture(material, aiTextureType_METALNESS);
                    desc.roughness = FirstTexture(material, aiTextureType_DIFFUSE_ROUGHNESS);
                }

                if (!IsSet(desc.albedo) && !IsSet(desc.metallic) && !IsSet(desc.roughness)) {
                    ApplyNonPbrRules(material, desc);
                }
                return desc;
            }

        private:
            static void ReadFactors(const aiMaterial &material, MaterialDesc &desc) {
                aiColor4D baseColor;
                aiColor3D diffuse;
                float opacity = 1.0f;
                if (material.Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
                    desc.baseColor = {baseColor.r, baseColor.g, baseColor.b, baseColor.a};
                } else {
                    if (material.Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                        desc.baseColor = {diffuse.r, diffuse.g, diffuse.b, 1.0f};
                    }
                    if (material.Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                        desc.baseColor.w = opacity;
                    }
                }

                material.Get(AI_MATKEY_METALLIC_FACTOR, desc.metallicFactor);
                material.Get(AI_MATKEY_ROUGHNESS_FACTOR, desc.roughnessFactor);
                aiColor3D emissive;
                if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                    desc.emissiveFactor = {emissive.r, emissive.g, emissive.b};
                }

                aiString alphaMode;
                if (material.Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
                    const std::string_view mode = alphaMode.C_Str();
                    desc.alphaMode = mode == "MASK"    ? AlphaMode::Mask
                                     : mode == "BLEND" ? AlphaMode::Blend
                                                       : AlphaMode::Opaque;
                    material.Get(AI_MATKEY_GLTF_ALPHACUTOFF, desc.alphaCutoff);
                } else if (desc.baseColor.w < 1.0f) {
                    desc.alphaMode = AlphaMode::Blend;
                }

                int twoSided = 0;
                if (material.Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) {
                    desc.doubleSided = twoSided != 0;
                }
            }

            // For formats without PBR textures (OBJ, older FBX): the diffuse map is the albedo, and the other maps are
            // found by name, as <material><suffix>.png or .jpg next to the model.
            void ApplyNonPbrRules(const aiMaterial &material, MaterialDesc &desc) {
                desc.albedo = FirstTexture(material, aiTextureType_DIFFUSE);
                if (IsSet(desc.albedo)) {
                    desc.baseColor = {1.0f, 1.0f, 1.0f, desc.baseColor.w};
                }

                const std::pair<TextureSource *, std::string_view> suffixes[] = {
                    {&desc.metallic, "_m"},   {&desc.roughness, "_r"}, {&desc.ambientOcclusion, "_ao"},
                    {&desc.normal, "_n"},     {&desc.emissive, "_e"},  {&desc.height, "_h"},
                    {&desc.opacityMap, "_d"},
                };
                for (const auto &[slot, suffix] : suffixes) {
                    if (!IsSet(*slot)) {
                        *slot = FindBySuffix(desc.name, suffix);
                    }
                }

                const auto &factor = desc.emissiveFactor;
                if (IsSet(desc.emissive) && factor.x == 0.0f && factor.y == 0.0f && factor.z == 0.0f) {
                    desc.emissiveFactor = {1.0f, 1.0f, 1.0f};
                }
            }

            [[nodiscard]] TextureSource FirstTexture(const aiMaterial &material, aiTextureType type) {
                aiString path;
                if (material.GetTextureCount(type) == 0 || material.GetTexture(type, 0, &path) != AI_SUCCESS) {
                    return {};
                }
                return Resolve(path);
            }

            // What a material's texture path points to: an image inside the file, or a file relative to the model
            [[nodiscard]] TextureSource Resolve(const aiString &path) {
                const std::string_view text(path.C_Str(), path.length);
                if (text.empty()) {
                    return {};
                }

                if (const aiTexture *texture = scene.GetEmbeddedTexture(path.C_Str())) {
                    if (texture->mHeight != 0) {
                        LogWarning("{}: texture \"{}\" is stored uncompressed in the file, which is not supported",
                                   source, text);
                        return {};
                    }
                    const auto [it, added] =
                        embeddedIndices.try_emplace(texture, static_cast<uint32_t>(model.embeddedTextures.size()));
                    if (added) {
                        const auto *bytes = reinterpret_cast<const std::byte *>(texture->pcData);
                        model.embeddedTextures.push_back(
                            {.bytes = {bytes, bytes + texture->mWidth}, .formatHint = texture->achFormatHint});
                    }
                    return EmbeddedTexture{it->second};
                }

                const std::optional<std::filesystem::path> file = PathFromUtf8(text);
                if (!file) {
                    LogWarning("{}: texture path \"{}\" is not valid UTF-8", source, text);
                    return {};
                }
                return NormalizedPath(directory / *file);
            }

            [[nodiscard]] TextureSource FindBySuffix(std::string_view materialName, std::string_view suffix) const {
                for (const std::string_view extension : {".png", ".jpg", ".jpeg"}) {
                    const std::optional<std::filesystem::path> file =
                        PathFromUtf8(std::format("{}{}{}", materialName, suffix, extension));
                    std::error_code ec;
                    if (file && std::filesystem::is_regular_file(directory / *file, ec)) {
                        return NormalizedPath(directory / *file);
                    }
                }
                return {};
            }
        };
    } // namespace

    std::expected<render::ModelData, core::Error> ImportModel(const std::filesystem::path &path) {
        const std::string source = core::ToUtf8(path.native());

        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            return std::unexpected(Error{ErrorCode::FileNotFound, std::format("{}: file not found", source)});
        }

        Assimp::Importer importer;
        const std::string extension = core::ToUtf8(path.extension().native());
        if (!importer.IsExtensionSupported(extension.c_str())) {
            return std::unexpected(
                Error{ErrorCode::UnsupportedFormat, std::format("{}: Assimp cannot read {} files", source, extension)});
        }

        const aiScene *scene = importer.ReadFile(source.c_str(), kImportFlags);
        if (scene == nullptr) {
            return std::unexpected(
                Error{ErrorCode::DecodeFailed, std::format("{}: Assimp: {}", source, importer.GetErrorString())});
        }
        if ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr || !scene->HasMeshes()) {
            return std::unexpected(Error{ErrorCode::DecodeFailed, std::format("{}: the file has no meshes", source)});
        }

        ModelData model;
        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            AddSubmesh(*scene->mMeshes[i], source, model.mesh);
        }

        if (model.mesh.indices.empty()) {
            return std::unexpected(
                Error{ErrorCode::DecodeFailed, std::format("{}: the file has no triangles", source)});
        }
        AddNodes(*scene->mRootNode, DirectX::XMMatrixIdentity(), model.nodes);

        MaterialReader materials{
            .scene = *scene, .directory = NormalizedPath(path).parent_path(), .source = source, .model = model};
        model.materials.reserve(scene->mNumMaterials);
        for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
            model.materials.push_back(materials.Read(*scene->mMaterials[i]));
        }
        return model;
    }

    bool IsModelFile(const std::filesystem::path &path) {
        std::wstring extension = path.extension().native();
        std::ranges::transform(extension, extension.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return extension == L".gltf" || extension == L".glb" || extension == L".obj" || extension == L".fbx";
    }
} // namespace engine::assets