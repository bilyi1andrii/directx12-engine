#ifndef D3D12_ENGINE_COMMANDLINE_H
#define D3D12_ENGINE_COMMANDLINE_H

#include <string>
#include <vector>

namespace engine::core {
    // The process's command-line arguments as UTF-8, without the program name (argv[1..]).
    // Split with the same quoting rules as the C runtime's argv.
    [[nodiscard]] std::vector<std::string> CommandLineArgs();
} // namespace engine::core

#endif // D3D12_ENGINE_COMMANDLINE_H
