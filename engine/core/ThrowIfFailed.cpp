#include "core/ThrowIfFailed.h"
#include <format>
#include <system_error>

namespace core {
    HResultError::HResultError(HRESULT hr, const std::string &message) : std::runtime_error(message), m_hr(hr) {}

    void ThrowHResult(HRESULT hr, std::source_location location) {
        throw HResultError(hr, std::format("HRESULT 0x{:08X}: {}\n  at {}:{}", static_cast<unsigned>(hr),
                                           std::system_category().message(hr), location.file_name(), location.line()));
    }

    void ThrowIfWin32Failed(bool succeeded, std::source_location location) {
        if (succeeded) {
            return;
        }
        const DWORD error = ::GetLastError();
        ThrowHResult(error != ERROR_SUCCESS ? HRESULT_FROM_WIN32(error) : E_FAIL, location);
    }
} // namespace core
