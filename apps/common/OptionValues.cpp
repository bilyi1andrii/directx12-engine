#include "OptionValues.h"

#include <charconv>
#include <system_error>

namespace engine::apps {
    std::optional<uint32_t> ParseUint32(std::string_view text) noexcept {
        // from_chars rejects signs, spaces and values that do not fit; `end` catches trailing characters
        uint32_t value = 0;
        const char *end = text.data() + text.size();
        const auto [last, error] = std::from_chars(text.data(), end, value);
        if (error != std::errc{} || last != end) {
            return std::nullopt;
        }
        return value;
    }
} // namespace engine::apps
