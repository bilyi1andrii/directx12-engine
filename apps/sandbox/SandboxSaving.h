#ifndef D3D12_ENGINE_SANDBOXSAVING_H
#define D3D12_ENGINE_SANDBOXSAVING_H

// The sandbox's save: whether a run uses it, reading it at the start, and writing where the user moved things when
// the sandbox closes.

#include <optional>
#include <span>

#include "SandboxGrid.h"
#include "SandboxModels.h"
#include "SandboxOptions.h"
#include "sandbox/SceneSave.h"
#include "scene/Scene.h"

namespace sandbox {
    [[nodiscard]] bool UsesSave(const SandboxOptions &options) noexcept;

    [[nodiscard]] std::optional<SandboxSave> ReadSave();

    void RestoreNamedCubes(scene::Scene &scene, std::span<const scene::ObjectId> cubes, const SandboxSave &saved);

    void WriteSave(const scene::Scene &scene, const SandboxGrid &grid, std::span<const scene::ObjectId> namedCubes,
                   const SandboxModels &models);
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXSAVING_H
