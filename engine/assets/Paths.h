#ifndef D3D12_ENGINE_PATHS_H
#define D3D12_ENGINE_PATHS_H

// Turns a path into one key per file for the caches: absolute, normalized and in the letter case on disk.

#include <filesystem>

namespace assets {
    [[nodiscard]] std::filesystem::path NormalizedPath(const std::filesystem::path &path);
} // namespace assets

#endif // D3D12_ENGINE_PATHS_H
