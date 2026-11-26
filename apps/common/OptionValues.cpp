#include "OptionValues.h"

#include <charconv>
#include <system_error>

namespace apps {
    std::optional<uint32_t> ParseUint32(std::string_view text) noexcept {
        uint32_t value = 0;
        const char *end = text.data() + text.size();
        const auto [last, error] = std::from_chars(text.data(), end, value);
        if (error != std::errc{} || last != end) {
            return std::nullopt;
        }
        return value;
    }
} // namespace apps
