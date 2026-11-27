#ifndef D3D12_ENGINE_BENCHAPP_H
#define D3D12_ENGINE_BENCHAPP_H

// The benchmark run: flies a scripted camera over a grid of copies of one model and times every frame, on the CPU
// and on the GPU. Warm-up frames are rendered first and not measured.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "BenchOptions.h"
#include "StressGrid.h"
#include "bench/BenchOutput.h"
#include "bench/GpuTimeCollector.h"
#include "common/AppChecks.h"
#include "common/AppShell.h"
#include "renderdata/Lights.h"
#include "renderdata/Model.h"
#include "renderdata/RenderList.h"
#include "renderdata/RendererSettings.h"
#include "scene/Camera.h"
#include "scene/CameraPath.h"

namespace bench {
    struct BenchRun {
        apps::RunResult result;
        std::vector<FrameTiming> timings;
        std::vector<std::string> gpuPasses;
        std::string adapterName;
        std::string driverVersion;
        uint32_t width = 0;
        uint32_t height = 0;
        bool tearingSupported = false;
        size_t objectCount = 0;
        size_t partCount = 0;
        size_t transparentPartCount = 0;
        size_t transparentDrawsPerObject = 0;
        uint64_t triangleCount = 0;
        std::string lights;
    };

    class BenchApp {
    public:
        BenchApp(apps::AppShell &shell, const BenchOptions &options, const render::Model &model);

        BenchApp(const BenchApp &) = delete;
        BenchApp &operator=(const BenchApp &) = delete;
        BenchApp(BenchApp &&) = delete;
        BenchApp &operator=(BenchApp &&) = delete;

        [[nodiscard]] std::expected<BenchRun, std::string> Run();

    private:
        void Measure();
        void HandleKeys();
        void PlaceCamera(uint64_t frame);
        void RenderFrame(uint64_t frame);

        [[nodiscard]] uint64_t TotalFrames() const noexcept {
            return uint64_t{m_options.warmupFrames} + m_options.frames;
        }

        apps::AppShell &m_shell;
        const BenchOptions &m_options;
        StressGrid m_grid;
        std::array<scene::CameraKeyframe, 5> m_path;
        scene::Camera m_camera;
        render::RendererSettings m_settings;
        render::LightSet m_lights;
        render::RenderList m_renderList;
        BenchRun m_run;
        GpuTimeCollector m_gpuTimes;
        uint64_t m_frame = 0;
    };
} // namespace bench

#endif // D3D12_ENGINE_BENCHAPP_H
