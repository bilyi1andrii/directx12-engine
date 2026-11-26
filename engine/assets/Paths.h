#ifndef D3D12_ENGINE_PATHS_H
#define D3D12_ENGINE_PATHS_H

#include <filesystem>

namespace assets {
    // Absolute and normalized: one file, one key for the asset caches. weakly_canonical also resolves "..",
    // links and the letter case on disk ("B.png" = "b.png"); falls back to absolute + lexically_normal.
    [[nodiscard]] std::filesystem::path NormalizedPath(const std::filesystem::path &path);
} // namespace assets

#endif // D3D12_ENGINE_PATHS_H
