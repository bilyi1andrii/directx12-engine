#ifndef D3D12_ENGINE_UNIQUEHANDLE_H
#define D3D12_ENGINE_UNIQUEHANDLE_H

// Owns a Windows handle, such as an event, and closes it when it goes out of scope.

#include <memory>

#include <Windows.h>

namespace core {
    struct HandleCloser {
        void operator()(HANDLE handle) const noexcept { ::CloseHandle(handle); }
    };

    using UniqueHandle = std::unique_ptr<void, HandleCloser>;
} // namespace core

#endif //D3D12_ENGINE_UNIQUEHANDLE_H
