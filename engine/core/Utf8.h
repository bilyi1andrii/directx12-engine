#ifndef D3D12_ENGINE_UTF8_H
#define D3D12_ENGINE_UTF8_H

#include <filesystem>
#include <string>
#include <string_view>

namespace engine::core {
    // UTF-16 (Windows API strings, paths) to UTF-8 (log, std::format, error messages).
    // Invalid UTF-16 becomes U+FFFD instead of failing.
    [[nodiscard]] std::string ToUtf8(std::wstring_view text);

    // UTF-8 text (command-line arguments, paths inside asset files) to a path. A plain std::string would be
    // read in the ANSI code page and break non-ASCII names. Throws std::system_error on invalid UTF-8.
    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view text);
} // namespace engine::core

#endif // D3D12_ENGINE_UTF8_H
