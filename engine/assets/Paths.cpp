#include "assets/Paths.h"

#include <system_error>

namespace assets {
    std::filesystem::path NormalizedPath(const std::filesystem::path &path) {
        std::error_code ec;
        const std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
        return ec ? path.lexically_normal() : result;
    }
} // namespace assets
