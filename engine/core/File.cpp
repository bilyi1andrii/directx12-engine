#include "core/File.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <system_error>

#include <Windows.h>

#include "core/ThrowIfFailed.h"
#include "core/Utf8.h"

namespace core {
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

    std::expected<std::vector<std::byte>, Error> ReadBinaryFile(const std::filesystem::path &path) {
        // Not path.string(): it converts to the ANSI code page, which garbles non-ASCII characters in
        // the UTF-8 log and throws for characters the code page does not have
        std::error_code error;
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error) {
            const ErrorCode code =
                error == std::errc::no_such_file_or_directory ? ErrorCode::FileNotFound : ErrorCode::ReadFailed;
            return std::unexpected(Error{code, std::format("{}: {}", ToUtf8(path.native()), error.message())});
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected(
                Error{ErrorCode::ReadFailed, std::format("{}: cannot open the file", ToUtf8(path.native()))});
        }

        std::vector<std::byte> data(static_cast<size_t>(size));
        file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file) {
            return std::unexpected(Error{ErrorCode::ReadFailed, std::format("{}: read failed", ToUtf8(path.native()))});
        }
        return data;
    }
} // namespace core
