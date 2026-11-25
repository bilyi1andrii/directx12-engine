#ifndef D3D12_ENGINE_THROWIFFAILED_H
#define D3D12_ENGINE_THROWIFFAILED_H

// Turns failed Windows and D3D12 calls into exceptions that say what failed and where.

#include <source_location>

#include <Windows.h>

namespace core {
    void ThrowHResult(HRESULT hr, std::source_location location);

    inline void ThrowIfFailed(HRESULT hr, std::source_location location = std::source_location::current()) {
        if (FAILED(hr)) {
            ThrowHResult(hr, location);
        }
    }

    void ThrowIfWin32Failed(bool succeeded, std::source_location location = std::source_location::current());

} // namespace core

#endif //D3D12_ENGINE_THROWIFFAILED_H
