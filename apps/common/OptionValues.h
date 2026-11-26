#ifndef D3D12_ENGINE_OPTIONVALUES_H
#define D3D12_ENGINE_OPTIONVALUES_H

// Helpers for the apps' command-line parsers.

#include <cstdint>
#include <optional>
#include <string_view>

namespace apps {
    [[nodiscard]] std::optional<uint32_t> ParseUint32(std::string_view text) noexcept;
} // namespace apps

#endif // D3D12_ENGINE_OPTIONVALUES_H
