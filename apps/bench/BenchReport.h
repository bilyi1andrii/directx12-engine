#ifndef D3D12_ENGINE_BENCHREPORT_H
#define D3D12_ENGINE_BENCHREPORT_H

// The end of a benchmark run: logs the medians and writes the results, or says why nothing was written. A run that
// stopped early, lost GPU times or could not write its files fails.

#include <chrono>
#include <expected>
#include <string>

#include "BenchApp.h"
#include "BenchOptions.h"
#include "rhi/ValidationStats.h"

namespace bench {
    inline constexpr int kExitBenchFailed = 1;

    [[nodiscard]] int ReportRun(const BenchOptions &options, const std::expected<BenchRun, std::string> &outcome,
                                std::chrono::system_clock::time_point started, const rhi::ValidationStats &validation);
} // namespace bench

#endif // D3D12_ENGINE_BENCHREPORT_H
