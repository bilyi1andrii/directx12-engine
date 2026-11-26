#ifndef D3D12_ENGINE_FILE_H
#define D3D12_ENGINE_FILE_H

// Finds the folder of the running program, where its runtime files live, and reads whole files into memory.

#include <cstddef>
#include <expected>
#include <filesystem>
#include <vector>

#include "core/Error.h"

namespace core {
    [[nodiscard]] std::filesystem::path ExecutableDirectory();

    [[nodiscard]] std::expected<std::vector<std::byte>, Error> ReadBinaryFile(const std::filesystem::path &path);
} // namespace core

#endif // D3D12_ENGINE_FILE_H
