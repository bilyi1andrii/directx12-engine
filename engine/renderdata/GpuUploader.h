#ifndef D3D12_ENGINE_GPUUPLOADER_H
#define D3D12_ENGINE_GPUUPLOADER_H

// What loading assets needs from the renderer: creating meshes, textures and materials.

#include <string_view>
#include <vector>

#include "renderdata/Handles.h"
#include "renderdata/Material.h"
#include "renderdata/MeshData.h"
#include "renderdata/TextureData.h"

namespace render {
    class GpuUploader {
    public:
        virtual ~GpuUploader() = default;

        // one handle per submesh, all share one vertex and index buffer
        [[nodiscard]] virtual std::vector<MeshHandle> CreateMesh(const MeshData &mesh, std::wstring_view name) = 0;

        [[nodiscard]] virtual TextureHandle CreateTexture(const TextureData &data, std::wstring_view name) = 0;

        // grey fallback for textures that failed to load
        [[nodiscard]] virtual TextureHandle UnloadedTexture() const noexcept = 0;

        [[nodiscard]] virtual MaterialHandle CreateMaterial(const MaterialInfo &material) = 0;

    // protected copy prevents slicing through base
    protected:
        GpuUploader() = default;
        GpuUploader(const GpuUploader &) = default;
        GpuUploader &operator=(const GpuUploader &) = default;
    };
} // namespace render

#endif // D3D12_ENGINE_GPUUPLOADER_H
