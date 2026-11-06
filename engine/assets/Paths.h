#ifndef D3D12_ENGINE_PATHS_H
#define D3D12_ENGINE_PATHS_H

#include <filesystem>

namespace engine::assets {
    // Absolute and normalized, so one file always gives the same path: what asset caches deduplicate by.
    // weakly_canonical also resolves "..", links and, for files that exist, the letter case on disk (MSVC), so
    // "B.png" and "b.png" give one path. Falls back to absolute + lexically_normal if that fails.
    [[nodiscard]] std::filesystem::path NormalizedPath(const std::filesystem::path &path);
} // namespace engine::assets

#endif // D3D12_ENGINE_PATHS_H
