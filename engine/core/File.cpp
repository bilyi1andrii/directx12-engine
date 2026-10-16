#include "core/File.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <system_error>

#include <Windows.h>

#include "core/ThrowIfFailed.h"

namespace engine::core {
    std::filesystem::path ExecutableDirectory() {
        // GetModuleFileNameW truncates silently when the buffer is too small (it returns the buffer
        // size), so grow it until the whole path fits
        std::wstring buffer(MAX_PATH, L'\0');
        while (true) {
            const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            ThrowIfWin32Failed(length != 0);
            if (length < buffer.size()) {
                buffer.resize(length);
                return std::filesystem::path(buffer).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    std::expected<std::vector<std::byte>, std::string> ReadBinaryFile(const std::filesystem::path &path) {
        std::error_code error;
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error) {
            return std::unexpected(std::format("{}: {}", path.string(), error.message()));
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(std::format("{}: cannot open the file", path.string()));
        }

        std::vector<std::byte> data(static_cast<size_t>(size));
        file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file) {
            return std::unexpected(std::format("{}: read failed", path.string()));
        }
        return data;
    }
} // namespace engine::core
