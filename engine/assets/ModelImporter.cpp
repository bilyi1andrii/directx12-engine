#include "assets/ModelImporter.h"

#include <cstddef>
#include <cstdint>
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

        // Flags:
        // - Triangulate: polygons become triangles, the only primitive the renderer draws.
        // - ConvertToLeftHanded: D3D's left-handed space, clockwise front faces and UVs with v down.
        // - GenSmoothNormals: only for meshes that have no normals.
        // - CalcTangentSpace: tangents for normal mapping later; needs normals and UVs.
        constexpr unsigned kImportFlags = aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
                                          aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;

        // Appends `mesh` to the model's shared vertex and index arrays as one submesh.
        // Its indices stay relative to its own first vertex (baseVertex), as Assimp stores them.
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
                // Missing only on meshes of points or lines: GenSmoothNormals fills them for triangles
                if (mesh.HasNormals()) {
                    vertex.normal = {mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z};
                }
                // Missing when the mesh has no UVs (CalcTangentSpace needs them); DX11 uses +X then too
                vertex.tangent = mesh.HasTangentsAndBitangents()
                                     ? DirectX::XMFLOAT3{mesh.mTangents[i].x, mesh.mTangents[i].y, mesh.mTangents[i].z}
                                     : DirectX::XMFLOAT3{1.0f, 0.0f, 0.0f};
                geometry.vertices.push_back(vertex);
            }

            // After Triangulate a face has 3 indices, or fewer for points and lines, which are not drawn
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

        // Adds a MeshNode for every mesh of `node` and its children. `parent` is the
        // parent's node -> model transform.
        void AddNodes(const aiNode &node, DirectX::FXMMATRIX parent, std::vector<MeshNode> &nodes) {
            // Assimp matrices multiply column vectors (translation in the last column); the engine multiplies
            // row vectors, so each one is transposed. A child is placed in its parent's space: local * parent.
            const DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(
                DirectX::XMMatrixTranspose(DirectX::XMMATRIX(&node.mTransformation.a1)), parent);

            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                MeshNode meshNode{.submesh = node.mMeshes[i]}; // submesh i is aiMesh i
                DirectX::XMStoreFloat4x4(&meshNode.transform, transform);
                nodes.push_back(meshNode);
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                AddNodes(*node.mChildren[i], transform, nodes);
            }
        }

        // Absolute and normalized, so one file always gives the same path (AssetCache deduplicates by it).
        // weakly_canonical also resolves "..", links and, for files that exist, the letter case on disk.
        [[nodiscard]] std::filesystem::path NormalizedPath(const std::filesystem::path &path) {
            std::error_code ec;
            std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
            if (!ec) {
                return result;
            }
            result = std::filesystem::absolute(path, ec);
            return (ec ? path : result).lexically_normal();
        }

        // Assimp strings are UTF-8. Nothing if they are not: the path conversion would throw.
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

        // Reads the materials of one model. Textures in separate files become normalized paths relative to the
        // model's folder; textures stored in the file are copied into ModelData::embeddedTextures, each once.
        struct MaterialReader {
            const aiScene &scene;
            std::filesystem::path directory; // the model's folder
            std::string_view source;         // the model file, for warnings
            ModelData &model;
            std::unordered_map<const aiTexture *, uint32_t> embeddedIndices{};

            [[nodiscard]] MaterialDesc Read(const aiMaterial &material) {
                MaterialDesc desc;
                aiString name;
                if (material.Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                    desc.name = name.C_Str();
                }

                ReadFactors(material, desc);

                // Texture slots any kind of file can fill
                desc.normal = FirstTexture(material, aiTextureType_NORMALS);
                desc.emissive = FirstTexture(material, aiTextureType_EMISSIVE);
                desc.height = FirstTexture(material, aiTextureType_HEIGHT);
                desc.opacityMap = FirstTexture(material, aiTextureType_OPACITY);
                desc.ambientOcclusion = FirstTexture(material, aiTextureType_AMBIENT_OCCLUSION);
                if (!IsSet(desc.ambientOcclusion)) {
                    desc.ambientOcclusion = FirstTexture(material, aiTextureType_LIGHTMAP); // glTF occlusion
                }

                // PBR textures, as glTF and PBR exports state them. glTF packs metallic (blue) and roughness (green)
                // into one texture.
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
                // A PBR base colour carries the opacity in its alpha; otherwise the diffuse colour and the opacity
                // are separate
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

                // Each Get leaves the default in place when the file does not state the value
                material.Get(AI_MATKEY_METALLIC_FACTOR, desc.metallicFactor);
                material.Get(AI_MATKEY_ROUGHNESS_FACTOR, desc.roughnessFactor);
                aiColor3D emissive;
                if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                    desc.emissiveFactor = {emissive.r, emissive.g, emissive.b};
                }

                // The file's alpha mode when it states one (glTF), else transparent when not fully opaque
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

            // Materials without PBR textures (OBJ, most FBX): the diffuse texture, and the other maps as files next
            // to the model named after the material, such as brick_n.png for the normal map of "brick". The
            // file's own textures win over these.
            void ApplyNonPbrRules(const aiMaterial &material, MaterialDesc &desc) {
                desc.albedo = FirstTexture(material, aiTextureType_DIFFUSE);
                if (IsSet(desc.albedo)) {
                    // The texture is shown as it is, not tinted by the diffuse colour
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

                // Shown as it is, unless the file states an emissive colour to multiply it with
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

            // An image stored in the file ("*N" or its name), or a file path relative to the model
            [[nodiscard]] TextureSource Resolve(const aiString &path) {
                const std::string_view text(path.C_Str(), path.length);
                if (text.empty()) {
                    return {};
                }

                if (const aiTexture *texture = scene.GetEmbeddedTexture(path.C_Str())) {
                    // mHeight 0: compressed file bytes (png, jpg, ...), mWidth of them. Otherwise raw texels.
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

            // <material name><suffix>.png, .jpg or .jpeg next to the model, if one exists
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
        // Assimp takes UTF-8 paths and converts them back to UTF-16 to open the file on Windows
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

        // The scene belongs to the importer and is freed with it
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
        AddNodes(*scene->mRootNode, DirectX::XMMatrixIdentity(), model.nodes);

        // Material i is Assimp material i, as SubmeshData::materialIndex expects
        MaterialReader materials{
            .scene = *scene, .directory = NormalizedPath(path).parent_path(), .source = source, .model = model};
        model.materials.reserve(scene->mNumMaterials);
        for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
            model.materials.push_back(materials.Read(*scene->mMaterials[i]));
        }
        return model;
    }
} // namespace engine::assets
