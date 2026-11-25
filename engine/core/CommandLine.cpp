#include "core/CommandLine.h"

#include <memory>

#include <Windows.h>
#include <shellapi.h> // CommandLineToArgvW

#include "core/ThrowIfFailed.h"
#include "core/Utf8.h"

namespace core {
    namespace {
        // CommandLineToArgvW returns one LocalAlloc block that holds the array and the strings
        struct LocalFreeDeleter {
            void operator()(LPWSTR *argv) const noexcept { ::LocalFree(argv); }
        };
    } // namespace

    std::vector<std::string> CommandLineArgs() {
        int argc = 0;
        LPWSTR *argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        ThrowIfWin32Failed(argv != nullptr);
        const std::unique_ptr<LPWSTR, LocalFreeDeleter> owner(argv);

        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.push_back(ToUtf8(argv[i]));
        }
        return args;
    }
} // namespace core
