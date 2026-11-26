#include "core/Assert.h"

#include <cstdlib>

#include <Windows.h>

#include "core/Log.h"

namespace core::detail {
    void AssertFailed(std::string_view expression, std::string_view message, std::source_location location) {
        LogError("Assertion failed: {}{}{}\n  at {}:{}", expression, message.empty() ? "" : " - ", message,
                 location.file_name(), location.line());

        if (::IsDebuggerPresent()) {
            __debugbreak();
            return;
        }

        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        std::abort();
    }
} // namespace core::detail
