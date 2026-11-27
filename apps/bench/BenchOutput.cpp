#include "BenchOutput.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <system_error>

#include "core/Utf8.h"

namespace fs = std::filesystem;
namespace chrono = std::chrono;

namespace bench {
    namespace {
        [[nodiscard]] std::string PathText(const fs::path &path) {
            return core::ToUtf8(path.native());
        }

        [[nodiscard]] std::expected<fs::path, std::string> CreateNewFolder(const fs::path &folder) {
            std::error_code error;
            fs::create_directories(folder.parent_path(), error);
            if (error) {
                return std::unexpected(
                    std::format("cannot create '{}': {}", PathText(folder.parent_path()), error.message()));
            }
            constexpr int kMaxAttempts = 100;
            for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
                fs::path candidate = folder;
                if (attempt > 1) {
                    candidate += std::format("-{}", attempt);
                }
                if (fs::create_directory(candidate, error)) {
                    return candidate;
                }
                if (error) {
                    return std::unexpected(std::format("cannot create '{}': {}", PathText(candidate), error.message()));
                }
            }
            return std::unexpected(
                std::format("'{}' and {} numbered copies exist already", PathText(folder), kMaxAttempts - 1));
        }

        template <typename WriteFunction>
        [[nodiscard]] std::expected<void, std::string> WriteFile(const fs::path &path, WriteFunction write) {
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                return std::unexpected(std::format("cannot open '{}' for writing", PathText(path)));
            }
            write(file);
            file.close();
            if (!file) {
                return std::unexpected(std::format("cannot write '{}'", PathText(path)));
            }
            return {};
        }
    } // namespace

    std::string GpuPassColumn(std::string_view passName) {
        std::string column = "gpu_";
        for (const char c : passName) {
            const auto byte = static_cast<unsigned char>(c);
            column += std::isalnum(byte) ? static_cast<char>(std::tolower(byte)) : '_';
        }
        return column + "_ms";
    }

    void WriteFramesCsv(std::ostream &out, std::span<const std::string> gpuPasses,
                        std::span<const FrameTiming> timings) {
        out << "frame,cpu_frame_ms,cpu_record_ms,gpu_total_ms";
        for (const std::string &pass : gpuPasses) {
            out << ',' << GpuPassColumn(pass);
        }
        out << '\n';

        const size_t passCount = std::min(gpuPasses.size(), kMaxGpuPasses);
        for (size_t frame = 0; frame < timings.size(); ++frame) {
            const FrameTiming &timing = timings[frame];
            out << std::format("{},{:.4f},{:.4f},{:.4f}", frame, timing.cpuFrameMs, timing.cpuRecordMs,
                               timing.gpuTotalMs);
            for (size_t pass = 0; pass < passCount; ++pass) {
                out << std::format(",{:.4f}", timing.gpuPassMs[pass]);
            }
            out << '\n';
        }
    }

    void WriteRunInfo(std::ostream &out, std::span<const RunField> fields) {
        for (const RunField &field : fields) {
            out << field.key << ": " << field.value << '\n';
        }
    }

    std::string FolderTimestamp(chrono::system_clock::time_point time) {
        const chrono::zoned_time local{chrono::current_zone(), chrono::floor<chrono::seconds>(time)};
        return std::format("{:%Y%m%d-%H%M%S}", local);
    }

    std::expected<fs::path, std::string> WriteResults(const fs::path &outDir, const fs::path &scene,
                                                      std::string_view timestamp,
                                                      std::span<const std::string> gpuPasses,
                                                      std::span<const FrameTiming> timings,
                                                      std::span<const RunField> fields) {
        const std::expected<fs::path, std::string> folder = CreateNewFolder(outDir / kEngineName / scene / timestamp);
        if (!folder) {
            return std::unexpected(folder.error());
        }
        if (auto written =
                WriteFile(*folder / "frames.csv", [&](std::ostream &out) { WriteFramesCsv(out, gpuPasses, timings); });
            !written) {
            return std::unexpected(written.error());
        }
        if (auto written = WriteFile(*folder / "run.txt", [&](std::ostream &out) { WriteRunInfo(out, fields); });
            !written) {
            return std::unexpected(written.error());
        }
        return *folder;
    }
} // namespace bench
