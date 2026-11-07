#ifndef D3D12_ENGINE_OPTIONVALUES_H
#define D3D12_ENGINE_OPTIONVALUES_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

// Values of command-line options, shared by the apps' option parsers (sandbox, bench)
namespace engine::apps {
    // A whole number that fits uint32_t, nothing else (no sign, spaces or trailing characters)
    [[nodiscard]] std::optional<uint32_t> ParseUint32(std::string_view text) noexcept;

    // A path from a UTF-8 argument (core::CommandLineArgs gives UTF-8), so non-ASCII names survive
    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view text);
} // namespace engine::apps

#endif // D3D12_ENGINE_OPTIONVALUES_H
