#include "core/Utf8.h"

#include <Windows.h>

#include "core/ThrowIfFailed.h"

namespace core {
    std::string ToUtf8(std::wstring_view text) {
        if (text.empty()) {
            return {};
        }
        const int length = static_cast<int>(text.size());
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
        ThrowIfWin32Failed(size > 0);
        std::string result(static_cast<size_t>(size), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr);
        return result;
    }

    std::filesystem::path PathFromUtf8(std::string_view text) {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
} // namespace core
