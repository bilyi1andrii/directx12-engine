#include "core/Log.h"

#include <cstdio>
#include <string>

#include <Windows.h>

namespace engine::core
{
    namespace
    {
        std::string_view LevelTag(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Info:
                return "info";
            case LogLevel::Warning:
                return "warning";
            case LogLevel::Error:
                return "error";
            }
            return "?";
        }
    } // namespace

    void WriteLog(LogLevel level, std::string_view message)
    {
        const std::string line = std::format("[{}] {}\n", LevelTag(level), message);

        // Debugger output
        ::OutputDebugStringA(line.c_str());

        // stderr: shown by the console or IDE terminal that started us
        std::fputs(line.c_str(), stderr);
        std::fflush(stderr);
    }
} // engine::core
