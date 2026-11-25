#ifndef D3D12_ENGINE_THROWIFFAILED_H
#define D3D12_ENGINE_THROWIFFAILED_H

#include <source_location>
#include <stdexcept>
#include <string>

#include <Windows.h>

namespace core {
    class HResultError : public std::runtime_error {
    public:
        HResultError(HRESULT hr, const std::string &message);
        [[nodiscard]] HRESULT Code() const noexcept { return m_hr; }

    private:
        HRESULT m_hr;
    };

    void ThrowHResult(HRESULT hr, std::source_location location);

    inline void ThrowIfFailed(HRESULT hr, std::source_location location = std::source_location::current()) {
        if (FAILED(hr)) {
            ThrowHResult(hr, location);
        }
    }

    void ThrowIfWin32Failed(bool succeeded, std::source_location location = std::source_location::current());

} // namespace core

#endif //D3D12_ENGINE_THROWIFFAILED_H
