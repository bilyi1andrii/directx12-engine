# bench_output: writing frames.csv and run.txt. CPU only (links engine_core alone), so engine_tests can test it.
add_library(bench_output STATIC
        BenchOutput.cpp
        BenchOutput.h)

engine_target_defaults(bench_output)

# PUBLIC: included as "bench/BenchOutput.h" (by the tests; the bench itself includes it from its own folder)
target_include_directories(bench_output PUBLIC "${PROJECT_SOURCE_DIR}/apps")

target_link_libraries(bench_output PRIVATE
        engine_core)

add_executable(bench
        main.cpp
        BenchOptions.cpp
        BenchOptions.h)

# Like the sandbox: a GUI exe only in Release, a console exe otherwise, so terminals and IDEs show the log
set_target_properties(bench PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
target_link_options(bench PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(bench)

# The build configuration (Debug, Release, ...) as a string literal, written into run.txt
target_compile_definitions(bench PRIVATE "ENGINE_BUILD_CONFIG=\"$<CONFIG>\"")

target_link_libraries(bench PRIVATE
        app_common
        bench_output
        engine_core
        engine_rhi
        engine_render
        engine_scene)

add_dependencies(bench compile_shaders)
engine_copy_agility_runtime(bench)
engine_copy_asan_runtime(bench)

# Smoke test: a short run of the dynamic grid (the path that uploads every frame) into the build folder. Fails on
# debug-layer errors or leaks (builds with the debug layer), a crash, an early exit, or results that could not be
# written (the bench exits with 1 if frames.csv or run.txt is missing). Not a measurement: Debug numbers mean nothing.
add_test(NAME bench_smoke
        COMMAND bench --grid dynamic --count 1000 --frames 5 --warmup 1 --out "${CMAKE_BINARY_DIR}/bench_smoke")
set_tests_properties(bench_smoke PROPERTIES TIMEOUT 30)
