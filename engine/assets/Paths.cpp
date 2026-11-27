#include "assets/Paths.h"

#include <algorithm>
#include <cwctype>
#include <system_error>

namespace assets {
    std::filesystem::path NormalizedPath(const std::filesystem::path &path) {
        std::error_code ec;
        const std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
        return ec ? path.lexically_normal() : result;
    }

    std::wstring LowerCaseExtension(const std::filesystem::path &path) {
        std::wstring extension = path.extension().native();
        std::ranges::transform(extension, extension.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return extension;
    }
} // namespace assets
