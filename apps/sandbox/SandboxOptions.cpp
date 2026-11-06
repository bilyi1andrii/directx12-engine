#include "SandboxOptions.h"

#include <charconv>
#include <format>

namespace {
    // A whole number that fits uint32_t, nothing else. from_chars rejects signs, spaces and values that do
    // not fit; `end` catches trailing characters.
    std::optional<uint32_t> ParseUint32(std::string_view text) {
        uint32_t value = 0;
        const char *end = text.data() + text.size();
        const auto [last, error] = std::from_chars(text.data(), end, value);
        if (error != std::errc{} || last != end) {
            return std::nullopt;
        }
        return value;
    }
} // namespace

std::expected<SandboxOptions, std::string> ParseSandboxOptions(std::span<const std::string> args) {
    SandboxOptions options;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--smoke-test") {
            if (options.smokeTestFrames) {
                return std::unexpected("--smoke-test is given more than once");
            }
            if (i + 1 == args.size()) {
                return std::unexpected("--smoke-test needs a frame count");
            }
            const std::string_view value = args[++i];
            const std::optional<uint32_t> frames = ParseUint32(value);
            if (!frames || *frames == 0) {
                return std::unexpected(std::format("--smoke-test needs a frame count >= 1, got '{}'", value));
            }
            options.smokeTestFrames = frames;
        } else if (arg == "--count") {
            if (options.gridCount) {
                return std::unexpected("--count is given more than once");
            }
            if (i + 1 == args.size()) {
                return std::unexpected("--count needs a number of cubes");
            }
            const std::string_view value = args[++i];
            const std::optional<uint32_t> count = ParseUint32(value);
            if (!count) {
                return std::unexpected(std::format("--count needs a number of cubes >= 0, got '{}'", value));
            }
            options.gridCount = count;
        } else if (arg == "--texture") {
            if (options.texturePath) {
                return std::unexpected("--texture is given more than once");
            }
            if (i + 1 == args.size() || args[i + 1].empty()) {
                return std::unexpected("--texture needs a file path");
            }
            // The arguments are UTF-8; a u8string tells the path so
            const std::string &value = args[++i];
            options.texturePath = std::filesystem::path(std::u8string(value.begin(), value.end()));
        } else if (arg == "--model") {
            if (options.modelPath) {
                return std::unexpected("--model is given more than once");
            }
            if (i + 1 == args.size() || args[i + 1].empty()) {
                return std::unexpected("--model needs a file path");
            }
            const std::string &value = args[++i];
            options.modelPath = std::filesystem::path(std::u8string(value.begin(), value.end()));
        } else {
            return std::unexpected(std::format("Unknown argument '{}'", arg));
        }
    }
    return options;
}
