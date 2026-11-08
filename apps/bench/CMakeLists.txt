# Writes the result files. Links only engine_core (no GPU), so the unit tests can use it.
add_library(bench_output STATIC
        BenchOutput.cpp
        BenchOutput.h)

engine_target_defaults(bench_output)

# The tests include it as "bench/BenchOutput.h"
target_include_directories(bench_output PUBLIC "${PROJECT_SOURCE_DIR}/apps")

target_link_libraries(bench_output PRIVATE
        engine_core)

add_executable(bench
        main.cpp
        BenchOptions.cpp
        BenchOptions.h
        StressGrid.cpp
        StressGrid.h)

# Like the sandbox: a GUI exe only in Release, a console exe otherwise, so terminals and IDEs show the log
set_target_properties(bench PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
target_link_options(bench PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(bench)

# The build name (Debug, Release, ...) for run.txt
target_compile_definitions(bench PRIVATE "ENGINE_BUILD_CONFIG=\"$<CONFIG>\"")

target_link_libraries(bench PRIVATE
        app_common
        bench_output
        # AssetCache: the model the grid draws
        engine_assets
        engine_core
        engine_rhi
        engine_render
        engine_scene)

add_dependencies(bench compile_shaders)
engine_copy_agility_runtime(bench)
engine_copy_asan_runtime(bench)

# Smoke test: a short run that uploads the grid every frame. Fails on debug-layer errors or leaks, a crash, an
# early exit, or results that could not be written. Not a measurement.
add_test(NAME bench_smoke
        COMMAND bench --grid dynamic --count 1000 --frames 5 --warmup 1 --out "${CMAKE_BINARY_DIR}/bench_smoke")
set_tests_properties(bench_smoke PROPERTIES TIMEOUT 30)
