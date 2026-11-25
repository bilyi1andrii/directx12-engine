#ifndef D3D12_ENGINE_ERROR_H
#define D3D12_ENGINE_ERROR_H

// A failure the caller can recover from, such as a missing or damaged file, with a message ready for the log.

#include <string>

namespace core {
    enum class ErrorCode {
        FileNotFound,
        ReadFailed,
        UnsupportedFormat,
        DecodeFailed,
    };

    struct Error {
        ErrorCode code;
        std::string message; // UTF-8
    };
} // namespace core

#endif // D3D12_ENGINE_ERROR_H
