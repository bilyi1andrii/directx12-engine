#include "SandboxGrid.h"

#include <cstddef>
#include <cstring>
#include <format>

#include <DirectXMath.h>

#include "core/File.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "core/Utf8.h"
#include "render/GpuResources.h"
#include "renderdata/GridLayout.h"
#include "renderdata/RenderList.h"

namespace fs = std::filesystem;

using core::LogInfo;
using core::LogWarning;

namespace sandbox {
    namespace {
        uint32_t ResolveCount(const SandboxOptions &options, const std::optional<SandboxSave> &saved) {
            if (options.gridCount) {
                return *options.gridCount;
            }
            return saved && saved->gridCount > 0 ? saved->gridCount : apps::kDefaultGridCount;
        }

        apps::GridMode ResolveMode(const SandboxOptions &options, const std::optional<SandboxSave> &saved) {
            if (options.gridMode) {
                return *options.gridMode;
            }
            return saved ? apps::ParseGridMode(saved->gridMode).value_or(apps::GridMode::Dynamic)
                         : apps::GridMode::Dynamic;
        }

        std::vector<DirectX::XMFLOAT4X4> GridPlacement(uint32_t side) {
            using namespace DirectX;

            std::vector<XMFLOAT4X4> worlds(size_t{side} * side);
            for (uint32_t row = 0; row < side; ++row) {
                for (uint32_t col = 0; col < side; ++col) {
                    const XMFLOAT3 offset =
                        render::GridCellOffset(row, col, side, side, apps::kGridSpacing, apps::kGridSpacing);
                    XMStoreFloat4x4(&worlds[size_t{row} * side + col],
                                    XMMatrixTranslation(offset.x, apps::kGridHeight, offset.z));
                }
            }
            return worlds;
        }
    } // namespace

    SandboxGrid::SandboxGrid(scene::Scene &scene, assets::AssetCache &assets, render::GpuResources &gpu,
                             const SandboxOptions &options, const std::optional<SandboxSave> &saved)
        : m_scene(scene), m_assets(assets), m_gpu(gpu), m_count(ResolveCount(options, saved)),
          m_mode(ResolveMode(options, saved)), m_side(apps::GridSide(m_count)),
          m_firstObject(static_cast<uint32_t>(scene.Objects().size())) {
        const fs::path texturePath = options.texturePath.value_or(core::ExecutableDirectory() / kDefaultGridTexture);
        AddCubes(m_assets.LoadTexture(texturePath, assets::TextureUsage::Diffuse));
        if (saved) {
            RestoreMovedCubes(*saved);
        }
        if (m_mode == apps::GridMode::Static) {
            render::RenderList staticObjects;
            scene::Extract(m_scene, staticObjects, scene::Mobility::Static);
            m_staticList = m_gpu.CreateStaticList(staticObjects, L"Grid/Instances");
        }
    }

    std::string SandboxGrid::Describe() const {
        return std::format("grid {} x {}, {}", m_side, m_side, apps::ToString(m_mode));
    }

    std::span<const render::StaticListHandle> SandboxGrid::StaticLists() const noexcept {
        if (!m_staticList) {
            return {};
        }
        return {&*m_staticList, 1};
    }

    std::vector<SavedGridCube> SandboxGrid::MovedCubes() const {
        std::vector<SavedGridCube> moved;
        if (m_mode != apps::GridMode::Dynamic) {
            return moved;
        }
        const std::vector<DirectX::XMFLOAT4X4> placement = GridPlacement(m_side);
        for (uint32_t i = 0; i < placement.size(); ++i) {
            const DirectX::XMFLOAT4X4 &world = m_scene.Object(Cube(i)).world;
            if (std::memcmp(&world, &placement[i], sizeof(world)) != 0) {
                moved.push_back({.index = i, .world = world});
            }
        }
        return moved;
    }

    void SandboxGrid::LoadDroppedTexture(const fs::path &path) {
        core::Timer timer;
        const std::string name = core::ToUtf8(path.filename().wstring());
        if (m_mode != apps::GridMode::Dynamic) {
            LogWarning("Dropped {}: a static grid keeps its texture (start with --grid dynamic)", name);
            return;
        }

        const render::TextureHandle texture = m_assets.LoadTexture(path, assets::TextureUsage::Diffuse);
        if (texture == m_gpu.UnloadedTexture()) {
            LogWarning("Dropped {}: neither a model (.gltf, .glb, .obj, .fbx) nor a readable image", name);
            return;
        }
        const render::MaterialHandle material = m_assets.Material({.albedo = texture});
        for (uint32_t i = 0; i < CubeCount(); ++i) {
            m_scene.SetObjectMaterial(Cube(i), material);
        }
        LogInfo("Dropped {}: grid texture, loaded in {:.0f} ms", name, timer.ElapsedMs());
    }

    void SandboxGrid::AddCubes(render::TextureHandle texture) {
        const render::Model &cube = m_assets.CubeModel(texture);
        const scene::Mobility mobility =
            m_mode == apps::GridMode::Static ? scene::Mobility::Static : scene::Mobility::Movable;
        const std::vector<DirectX::XMFLOAT4X4> placement = GridPlacement(m_side);
        for (const DirectX::XMFLOAT4X4 &world : placement) {
            m_scene.AddObject("Grid cube", cube, world, mobility);
        }
        LogInfo("Grid: {} x {} = {} cubes, {}", m_side, m_side, placement.size(), apps::ToString(m_mode));
    }

    void SandboxGrid::RestoreMovedCubes(const SandboxSave &saved) {
        if (saved.gridCubes.empty()) {
            return;
        }
        if (m_mode == apps::GridMode::Dynamic && saved.gridCount == m_count) {
            for (const SavedGridCube &cube : saved.gridCubes) {
                if (cube.index < CubeCount()) {
                    m_scene.SetObjectWorld(Cube(cube.index), cube.world);
                }
            }
        } else {
            LogInfo("Scene: {} saved grid cubes left out (they belong to a dynamic grid of {} cubes)",
                    saved.gridCubes.size(), saved.gridCount);
        }
    }
} // namespace sandbox
