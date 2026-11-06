#include "assets/ModelImporter.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <DirectXMath.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/Log.h"
#include "core/Utf8.h"

namespace engine::assets {
    namespace {
        using core::Error;
        using core::ErrorCode;
        using core::LogWarning;
        using render::MeshData;
        using render::MeshNode;
        using render::ModelData;
        using render::SubmeshData;
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

        // One entry per Assimp material, so SubmeshData::materialIndex is valid; filled in later
        model.materials.resize(scene->mNumMaterials);
        for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
            aiString name;
            if (scene->mMaterials[i]->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                model.materials[i].name = name.C_Str();
            }
        }
        return model;
    }
} // namespace engine::assets
