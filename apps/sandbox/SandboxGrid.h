#ifndef D3D12_ENGINE_SANDBOXGRID_H
#define D3D12_ENGINE_SANDBOXGRID_H

// The sandbox's cube grid under the models: movable cubes, or cubes uploaded once that never move. Its size and kind
// come from the command line or the save. It keeps the cubes the user moved, and a movable grid can show a new image.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "SandboxOptions.h"
#include "assets/AssetCache.h"
#include "common/GridSetup.h"
#include "renderdata/Handles.h"
#include "sandbox/SceneSave.h"
#include "scene/Scene.h"

namespace render {
    class GpuResources;
} // namespace render

namespace sandbox {
    inline constexpr std::wstring_view kDefaultGridTexture = L"data/textures/Endstone.jpg";

    class SandboxGrid {
    public:
        SandboxGrid(scene::Scene &scene, assets::AssetCache &assets, render::GpuResources &gpu,
                    const SandboxOptions &options, const std::optional<SandboxSave> &saved);

        SandboxGrid(const SandboxGrid &) = delete;
        SandboxGrid &operator=(const SandboxGrid &) = delete;
        SandboxGrid(SandboxGrid &&) = delete;
        SandboxGrid &operator=(SandboxGrid &&) = delete;

        [[nodiscard]] apps::GridMode Mode() const noexcept { return m_mode; }
        [[nodiscard]] uint32_t Count() const noexcept { return m_count; }
        [[nodiscard]] std::string Describe() const;

        [[nodiscard]] std::span<const render::StaticListHandle> StaticLists() const noexcept;

        [[nodiscard]] std::vector<SavedGridCube> MovedCubes() const;

        void LoadDroppedTexture(const std::filesystem::path &path);

    private:
        [[nodiscard]] scene::ObjectId Cube(uint32_t index) const noexcept { return {.index = m_firstObject + index}; }
        [[nodiscard]] uint32_t CubeCount() const noexcept { return m_side * m_side; }

        void AddCubes(render::TextureHandle texture);
        void RestoreMovedCubes(const SandboxSave &saved);

        scene::Scene &m_scene;
        assets::AssetCache &m_assets;
        render::GpuResources &m_gpu;
        uint32_t m_count;
        apps::GridMode m_mode;
        uint32_t m_side;
        uint32_t m_firstObject;
        std::optional<render::StaticListHandle> m_staticList;
    };
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXGRID_H
