#ifndef D3D12_ENGINE_HELPERS_H
#define D3D12_ENGINE_HELPERS_H

#include <Windows.h>
#include <format>
#include <source_location>
#include <stdexcept>

inline void ThrowIfFailed(HRESULT hr, std::source_location loc =
                              std::source_location::current())
{
    if (FAILED(hr))
    {
        throw std::runtime_error(std::format("HRESULT 0x{:08X} at {}:{}",
                                             static_cast<unsigned>(hr), loc.file_name(),
                                             loc.line()));
    }
}

#endif //D3D12_ENGINE_HELPERS_H
