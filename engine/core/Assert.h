#ifndef D3D12_ENGINE_ASSERT_H
#define D3D12_ENGINE_ASSERT_H

#include <format>
#include <source_location>
#include <string_view>

namespace core::detail {
    // Logs the failed check, then breaks into the debugger, or aborts when no debugger is attached.
    void AssertFailed(std::string_view expression, std::string_view message, std::source_location location);
} // namespace core::detail

// ENGINE_ASSERT(condition)
// ENGINE_ASSERT_MSG(condition, "format {}", args...)
//
// Checks an assumption that can only be false if there is a bug in our code, e.g. "index < size" or
// "this function is only called after Init()". Debug builds stop in the debugger on the failing line.
// Release builds remove the check entirely and do not evaluate the condition.
// For failures that can happen at runtime (file not found, device removed) use ThrowIfFailed / std::expected.
#if defined(_DEBUG)
#define ENGINE_ASSERT(condition)                                                           \
    do {                                                                                   \
        if (!(condition))                                                                  \
            ::core::detail::AssertFailed(#condition, {}, std::source_location::current()); \
    } while (false)
#define ENGINE_ASSERT_MSG(condition, ...)                                                                        \
    do {                                                                                                         \
        if (!(condition))                                                                                        \
            ::core::detail::AssertFailed(#condition, std::format(__VA_ARGS__), std::source_location::current()); \
    } while (false)
#else
// sizeof(...) is never evaluated, but it still "uses" the variables in the condition, so variables that only
// asserts read do not cause unused-variable warnings (errors under /WX) in release builds.
#define ENGINE_ASSERT(condition) ((void)sizeof(!(condition)))
#define ENGINE_ASSERT_MSG(condition, ...) ((void)sizeof(!(condition)))
#endif

#endif //D3D12_ENGINE_ASSERT_H
