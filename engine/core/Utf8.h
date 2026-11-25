#ifndef D3D12_ENGINE_UTF8_H
#define D3D12_ENGINE_UTF8_H

// Converts Windows' UTF-16 strings to UTF-8 text, and UTF-8 text to paths that keep non-ASCII names.

#include <filesystem>
#include <string>
#include <string_view>

namespace core {
    [[nodiscard]] std::string ToUtf8(std::wstring_view text);

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view text);
} // namespace core

#endif // D3D12_ENGINE_UTF8_H
