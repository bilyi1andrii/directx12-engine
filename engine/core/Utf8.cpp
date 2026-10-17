#include "core/Utf8.h"

#include <Windows.h>

#include "core/ThrowIfFailed.h"

namespace engine::core {
    std::string ToUtf8(std::wstring_view text) {
        if (text.empty()) {
            return {};
        }
        // First call: the size of the result in bytes; second call: the conversion
        const int length = static_cast<int>(text.size());
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
        ThrowIfWin32Failed(size > 0);
        std::string result(static_cast<size_t>(size), '\0');
        ThrowIfWin32Failed(
            ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr) == size);
        return result;
    }
} // namespace engine::core
