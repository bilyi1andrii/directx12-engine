#ifndef D3D12_ENGINE_GPUUPLOADER_H
#define D3D12_ENGINE_GPUUPLOADER_H

#include <string_view>
#include <vector>

#include "renderdata/Handles.h"
#include "renderdata/MeshData.h"
#include "renderdata/TextureData.h"

namespace engine::render {
    // What asset loading needs from the renderer: creating meshes and textures from CPU data. Renderer
    // implements it; AssetCache (engine/assets) only sees this interface, so it does not depend on
    // engine_render, and tests can pass a fake that needs no GPU.
    class GpuUploader {
    public:
        virtual ~GpuUploader() = default;

        // One MeshHandle per submesh of `mesh`, in submesh order
        [[nodiscard]] virtual std::vector<MeshHandle> CreateMesh(const MeshData &mesh, std::wstring_view name) = 0;

        [[nodiscard]] virtual TextureHandle CreateTexture(const TextureData &data, std::wstring_view name) = 0;

        // The grey texture for one that failed to load
        [[nodiscard]] virtual TextureHandle UnloadedTexture() const noexcept = 0;

    protected:
        GpuUploader() = default;
        GpuUploader(const GpuUploader &) = default;
        GpuUploader &operator=(const GpuUploader &) = default;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_GPUUPLOADER_H
