#include "core/Assert.h"

#include <cstdlib>

#include <Windows.h>

#include "core/Log.h"

namespace engine::core::detail {
    void AssertFailed(std::string_view expression, std::string_view message, std::source_location location) {
        LogError("Assertion failed: {}{}{}\n  at {}:{}", expression, message.empty() ? "" : " - ", message,
                 location.file_name(), location.line());

        if (::IsDebuggerPresent()) {
            __debugbreak();
            // the debugger stops here; step out to see the failing line
            return; // "continue" in the debugger resumes after the assert
        }

        // No debugger: end the process with exit code 3. Turn off the debug runtime's "abort() has been called"
        // dialog and Windows Error Reporting first.
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        std::abort();
    }
} // namespace engine::core::detail
