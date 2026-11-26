#ifndef D3D12_ENGINE_LOG_H
#define D3D12_ENGINE_LOG_H

// Writes log lines to the debugger output and to the console.

#include <format>
#include <string_view>
#include <utility>

namespace core {
    enum class LogLevel {
        Info,
        Warning,
        Error,
    };

    void WriteLog(LogLevel level, std::string_view message);

    template <typename... Args> void LogInfo(std::format_string<Args...> format, Args &&...args) {
        WriteLog(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args> void LogWarning(std::format_string<Args...> format, Args &&...args) {
        WriteLog(LogLevel::Warning, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args> void LogError(std::format_string<Args...> format, Args &&...args) {
        WriteLog(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
    }
} // namespace core

#endif //D3D12_ENGINE_LOG_H
