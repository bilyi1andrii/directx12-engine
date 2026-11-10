#ifndef D3D12_ENGINE_OPTIONVALUES_H
#define D3D12_ENGINE_OPTIONVALUES_H

#include <cstdint>
#include <optional>
#include <string_view>

// Helpers for the apps' command-line parsers
namespace engine::apps {
    // A whole number that fits uint32_t; nullopt for signs, spaces or extra characters
    [[nodiscard]] std::optional<uint32_t> ParseUint32(std::string_view text) noexcept;
} // namespace engine::apps

#endif // D3D12_ENGINE_OPTIONVALUES_H
