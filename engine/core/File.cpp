#include "core/File.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <system_error>

#include <Windows.h>

#include "core/ThrowIfFailed.h"
#include "core/Utf8.h"

namespace core {
    namespace {
        constexpr DWORD kMaxPathLength = 32767;
    } // namespace

    std::filesystem::path ExecutableDirectory() {
        std::wstring buffer(kMaxPathLength, L'\0');
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), kMaxPathLength);
        ThrowIfWin32Failed(length != 0);
        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
    }

    std::expected<std::vector<std::byte>, Error> ReadBinaryFile(const std::filesystem::path &path) {
        std::error_code error;
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error) {
            const ErrorCode code =
                error == std::errc::no_such_file_or_directory ? ErrorCode::FileNotFound : ErrorCode::ReadFailed;
            return std::unexpected(Error{code, std::format("{}: {}", ToUtf8(path.native()), error.message())});
        }

        std::ifstream file(path, std::ios::binary);
        std::vector<std::byte> data(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()))) {
            return std::unexpected(Error{ErrorCode::ReadFailed, std::format("{}: read failed", ToUtf8(path.native()))});
        }
        return data;
    }
} // namespace core
