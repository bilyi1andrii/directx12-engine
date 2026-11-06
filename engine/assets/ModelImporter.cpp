#include "assets/ModelImporter.h"

#include <format>
#include <string>
#include <system_error>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/Utf8.h"

namespace engine::assets {
    namespace {
        using core::Error;
        using core::ErrorCode;
        using render::ModelData;

        // Flags:
        // - Triangulate: polygons become triangles, the only primitive the renderer draws.
        // - ConvertToLeftHanded: D3D's left-handed space, clockwise front faces and UVs with v down.
        // - GenSmoothNormals: only for meshes that have no normals.
        // - CalcTangentSpace: tangents for normal mapping later; needs normals and UVs.
        constexpr unsigned kImportFlags = aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
                                          aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;
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
        return model;
    }
} // namespace engine::assets
