#ifndef D3D12_ENGINE_ASSERT_H
#define D3D12_ENGINE_ASSERT_H

// Checks for bugs in our own code. In Debug a failed check logs, then stops in the debugger or ends the process;
// Release builds remove the checks.

#include <format>
#include <source_location>
#include <string_view>

namespace core::detail {
    void AssertFailed(std::string_view expression, std::string_view message, std::source_location location);
} // namespace core::detail

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
#define ENGINE_ASSERT(condition) ((void)sizeof(!(condition)))
#define ENGINE_ASSERT_MSG(condition, ...) ((void)sizeof(!(condition)))
#endif

#endif //D3D12_ENGINE_ASSERT_H
