#ifndef D3D12_ENGINE_ERROR_H
#define D3D12_ENGINE_ERROR_H

#include <string>

namespace core {
    // What went wrong, for callers that react differently to each case
    enum class ErrorCode {
        FileNotFound,
        ReadFailed,        // the file exists but could not be read
        UnsupportedFormat, // no decoder for this kind of file, or a layout the engine does not handle
        DecodeFailed,      // the file is damaged, or decoding failed for another reason
    };

    // A recoverable failure, returned as std::expected<T, Error>. Bugs use ENGINE_ASSERT and unrecoverable
    // HRESULTs use ThrowIfFailed instead.
    struct Error {
        ErrorCode code;
        std::string message; // UTF-8, names the file and the reason: ready for the log
    };
} // namespace core

#endif // D3D12_ENGINE_ERROR_H
