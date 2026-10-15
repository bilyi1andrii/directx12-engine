#ifndef D3D12_ENGINE_UNIQUEHANDLE_H
#define D3D12_ENGINE_UNIQUEHANDLE_H

#include <memory>

#include <Windows.h>

namespace engine::core
{
    struct HandleCloser
    {
        void operator()(HANDLE handle) const noexcept
        {
            if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(handle);
            }
        }
    };

    // Owns a Win32 kernel HANDLE (event, file, thread, ...) and closes it on destruction.
    using UniqueHandle = std::unique_ptr<void, HandleCloser>;
} // namespace engine::core

#endif //D3D12_ENGINE_UNIQUEHANDLE_H
