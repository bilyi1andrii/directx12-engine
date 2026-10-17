#ifndef D3D12_ENGINE_UTF8_H
#define D3D12_ENGINE_UTF8_H

#include <string>
#include <string_view>

namespace engine::core {
    // UTF-16 (Windows API strings, paths) to UTF-8 (log, std::format, error messages).
    // Invalid UTF-16 becomes U+FFFD instead of failing.
    [[nodiscard]] std::string ToUtf8(std::wstring_view text);
} // namespace engine::core

#endif // D3D12_ENGINE_UTF8_H
