#ifndef D3D12_ENGINE_SANDBOXMODELS_H
#define D3D12_ENGINE_SANDBOXMODELS_H

// The models standing on the sandbox's grid, each scaled to the same size. They stand in a row along x, except the
// ones dropped onto the grid, which stand where they land.

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <DirectXMath.h>

#include "assets/AssetCache.h"
#include "sandbox/SceneSave.h"
#include "scene/Scene.h"

namespace sandbox {
    inline constexpr float kModelSize = 8.0f;
    inline constexpr float kModelSpacing = 10.0f;

    struct PlacedModel {
        scene::ObjectId id;
        std::filesystem::path path;
    };

    class SandboxModels {
    public:
        SandboxModels(scene::Scene &scene, assets::AssetCache &assets) : m_scene(scene), m_assets(assets) {}

        SandboxModels(const SandboxModels &) = delete;
        SandboxModels &operator=(const SandboxModels &) = delete;
        SandboxModels(SandboxModels &&) = delete;
        SandboxModels &operator=(SandboxModels &&) = delete;

        void Add(const std::filesystem::path &path);
        void AddSaved(std::span<const SavedModel> saved);
        void AddRow(std::span<const std::filesystem::path> paths);
        void AddDropped(const std::filesystem::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint);

        [[nodiscard]] std::span<const PlacedModel> Placed() const noexcept { return m_placed; }
        [[nodiscard]] bool Empty() const noexcept { return m_placed.empty(); }

    private:
        bool Place(const std::filesystem::path &path, float x, float z,
                   const std::optional<DirectX::XMFLOAT4X4> &savedWorld = std::nullopt);
        [[nodiscard]] std::optional<float> NextFreeX() const;
        void UpdateNextX();

        scene::Scene &m_scene;
        assets::AssetCache &m_assets;
        std::vector<PlacedModel> m_placed;
        float m_nextX = kModelSpacing;
    };
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXMODELS_H
