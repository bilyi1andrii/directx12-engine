#ifndef D3D12_ENGINE_LOG_H
#define D3D12_ENGINE_LOG_H

#include <format>
#include <string_view>
#include <utility>

namespace engine::core
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error,
    };

    // Writes one line to the debugger output and to stderr
    void WriteLog(LogLevel level, std::string_view message);

    template <typename... Args>
    void LogInfo(std::format_string<Args...> format, Args&&... args)
    {
        WriteLog(LogLevel::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void LogWarning(std::format_string<Args...> format, Args&&... args)
    {
        WriteLog(LogLevel::Warning, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void LogError(std::format_string<Args...> format, Args&&... args)
    {
        WriteLog(LogLevel::Error, std::format(format, std::forward<Args>(args)...));
    }
} // namespace engine::core


#endif //D3D12_ENGINE_LOG_H
