#ifndef D3D12_ENGINE_FILE_H
#define D3D12_ENGINE_FILE_H

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace engine::core {
    // Directory of the running .exe. Runtime files (shaders/, D3D12/) are found relative to it,
    // not to the working directory, which depends on how the program was started.
    [[nodiscard]] std::filesystem::path ExecutableDirectory();

    // Reads a whole file into memory. On failure the error names the file and the reason.
    [[nodiscard]] std::expected<std::vector<std::byte>, std::string> ReadBinaryFile(const std::filesystem::path &path);
} // namespace engine::core

#endif // D3D12_ENGINE_FILE_H
