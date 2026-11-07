#include "OptionValues.h"

#include <charconv>
#include <string>
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

    std::filesystem::path PathFromUtf8(std::string_view text) {
        // Through u8string, so the bytes are read as UTF-8, not in the ANSI code page
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
} // namespace engine::apps
