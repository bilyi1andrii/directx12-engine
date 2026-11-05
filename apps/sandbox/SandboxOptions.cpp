#include "SandboxOptions.h"

#include <charconv>
#include <format>

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
            // from_chars rejects signs, spaces and values that do not fit; `end` catches trailing characters
            uint32_t frames = 0;
            const char *end = value.data() + value.size();
            const auto [last, error] = std::from_chars(value.data(), end, frames);
            if (error != std::errc{} || last != end || frames == 0) {
                return std::unexpected(std::format("--smoke-test needs a frame count >= 1, got '{}'", value));
            }
            options.smokeTestFrames = frames;
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
        } else {
            return std::unexpected(std::format("Unknown argument '{}'", arg));
        }
    }
    return options;
}
