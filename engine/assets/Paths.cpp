#include "assets/Paths.h"

#include <system_error>

namespace assets {
    std::filesystem::path NormalizedPath(const std::filesystem::path &path) {
        std::error_code ec;
        std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
        if (!ec) {
            return result;
        }
        result = std::filesystem::absolute(path, ec);
        return (ec ? path : result).lexically_normal();
    }
} // namespace assets
