#include "SandboxModels.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "common/GridSetup.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "core/Utf8.h"
#include "renderdata/Model.h"

namespace fs = std::filesystem;

using core::LogInfo;
using core::LogWarning;

namespace sandbox {
    void SandboxModels::Add(const fs::path &path) {
        Place(path, 0.0f, 0.0f);
        UpdateNextX();
    }

    void SandboxModels::AddSaved(std::span<const SavedModel> saved) {
        for (const SavedModel &model : saved) {
            Place(model.path, 0.0f, 0.0f, model.world);
        }
        UpdateNextX();
    }

    void SandboxModels::AddRow(std::span<const fs::path> paths) {
        const std::optional<float> firstX = NextFreeX();
        for (size_t i = 0; i < paths.size(); ++i) {
            const float x = firstX ? *firstX + static_cast<float>(i) * kModelSpacing
                                   : (static_cast<float>(i) - static_cast<float>(paths.size() - 1) / 2) * kModelSpacing;
            Place(paths[i], x, 0.0f);
        }
        UpdateNextX();
    }

    void SandboxModels::AddDropped(const fs::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint) {
        core::Timer timer;
        const float x = dropPoint ? dropPoint->x : m_nextX;
        const float z = dropPoint ? dropPoint->z : 0.0f;
        if (Place(path, x, z)) {
            if (!dropPoint) {
                m_nextX += kModelSpacing;
            }
            LogInfo("Dropped {}: loaded in {:.0f} ms at ({:.1f}, {:.1f})", core::ToUtf8(path.filename().wstring()),
                    timer.ElapsedMs(), x, z);
        }
    }

    bool SandboxModels::Place(const fs::path &path, float x, float z,
                              const std::optional<DirectX::XMFLOAT4X4> &savedWorld) {
        using namespace DirectX;

        const auto model = m_assets.LoadModel(path);
        if (!model) {
            LogWarning("Model: {}", model.error().message);
            return false;
        }
        const render::Aabb &bounds = (*model)->bounds;
        const float extent =
            std::max({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z});
        const float scale = kModelSize / extent;
        const XMFLOAT3 pivot{(bounds.min.x + bounds.max.x) / 2, bounds.min.y, (bounds.min.z + bounds.max.z) / 2};
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, apps::kGridTop, z));
        std::string name = core::ToUtf8((path.parent_path().filename() / path.filename()).generic_wstring());
        LogInfo("Model {}: {} parts, largest side {:.2f} -> scaled by {:.4f}{}", name, (*model)->parts.size(), extent,
                scale, savedWorld ? " (placed as saved)" : "");
        const scene::ObjectId id =
            m_scene.AddObject(std::move(name), **model, savedWorld.value_or(world), scene::Mobility::Movable, pivot);
        m_placed.push_back({.id = id, .path = path});
        return true;
    }

    std::optional<float> SandboxModels::NextFreeX() const {
        std::optional<float> next;
        for (const PlacedModel &model : m_placed) {
            const float x = m_scene.ObjectPosition(model.id).x + kModelSpacing;
            next = next ? std::max(*next, x) : x;
        }
        return next;
    }

    void SandboxModels::UpdateNextX() {
        m_nextX = std::max(kModelSpacing, NextFreeX().value_or(kModelSpacing));
    }
} // namespace sandbox
