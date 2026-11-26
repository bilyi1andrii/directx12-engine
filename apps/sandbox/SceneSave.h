#ifndef D3D12_ENGINE_SCENESAVE_H
#define D3D12_ENGINE_SCENESAVE_H

// Reads and writes saves/sandbox.json: where the models and cubes the user moved stand.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "core/Error.h"

namespace sandbox {
    inline constexpr int kSaveVersion = 1;

    struct SavedGridCube {
        uint32_t index = 0;
        DirectX::XMFLOAT4X4 world{};
    };

    struct SavedCube {
        std::string name;
        DirectX::XMFLOAT4X4 world{};
    };

    struct SavedModel {
        std::filesystem::path path;
        DirectX::XMFLOAT4X4 world{};
    };

    struct SandboxSave {
        uint32_t gridCount = 0;
        std::string gridMode;
        std::vector<SavedGridCube> gridCubes;
        std::vector<SavedCube> cubes;
        std::vector<SavedModel> models;
    };

    [[nodiscard]] std::expected<SandboxSave, core::Error> ReadSandboxSave(const std::filesystem::path &file,
                                                                          const std::filesystem::path &root);

    [[nodiscard]] std::expected<void, std::string>
    WriteSandboxSave(const std::filesystem::path &file, const SandboxSave &save, const std::filesystem::path &root);
} // namespace sandbox

#endif // D3D12_ENGINE_SCENESAVE_H
