#ifndef D3D12_ENGINE_TESTFILES_H
#define D3D12_ENGINE_TESTFILES_H

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <system_error>
#include <utility>

namespace tests {
    // A file in the system temp folder, deleted at the end of the test. Tests use no asset files from the repo.
    class TempFile {
    public:
        TempFile(std::filesystem::path name, std::span<const std::byte> content)
            : m_path(std::filesystem::temp_directory_path() / std::move(name)) {
            std::ofstream file(m_path, std::ios::binary);
            file.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size()));
        }
        ~TempFile() {
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
        }

        TempFile(const TempFile &) = delete;
        TempFile &operator=(const TempFile &) = delete;

        [[nodiscard]] const std::filesystem::path &Path() const noexcept { return m_path; }

    private:
        std::filesystem::path m_path;
    };

    // A path in the temp folder that no test creates
    [[nodiscard]] inline std::filesystem::path MissingFile(const std::filesystem::path &name) {
        return std::filesystem::temp_directory_path() / name;
    }
} // namespace tests

#endif // D3D12_ENGINE_TESTFILES_H
