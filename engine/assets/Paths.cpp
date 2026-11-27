#include "assets/Paths.h"

#include <algorithm>
#include <cwctype>
#include <system_error>

namespace fs = std::filesystem;

namespace assets {
    fs::path NormalizedPath(const fs::path &path) {
        std::error_code ec;
        // absolute, .. resolved, letter case on disk
        const fs::path result = fs::weakly_canonical(path, ec);
        return ec ? path.lexically_normal() : result; // fallback, text only
    }

    std::wstring LowerCaseExtension(const fs::path &path) {
        std::wstring extension = path.extension().native();
        std::ranges::transform(extension, extension.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return extension;
    }
} // namespace assets
