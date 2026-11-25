#include "core/ThrowIfFailed.h"

#include <format>
#include <stdexcept>
#include <system_error>

namespace core {
    void ThrowHResult(HRESULT hr, std::source_location location) {
        throw std::runtime_error(std::format("HRESULT 0x{:08X}: {}\n  at {}:{}", static_cast<unsigned>(hr),
                                             std::system_category().message(hr), location.file_name(),
                                             location.line()));
    }

    void ThrowIfWin32Failed(bool succeeded, std::source_location location) {
        if (!succeeded) {
            ThrowHResult(HRESULT_FROM_WIN32(::GetLastError()), location);
        }
    }
} // namespace core
