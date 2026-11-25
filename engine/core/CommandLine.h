#ifndef D3D12_ENGINE_COMMANDLINE_H
#define D3D12_ENGINE_COMMANDLINE_H

// The program's command-line arguments as UTF-8 text, without the program's own name.

#include <string>
#include <vector>

namespace core {
    [[nodiscard]] std::vector<std::string> CommandLineArgs();
} // namespace core

#endif // D3D12_ENGINE_COMMANDLINE_H
