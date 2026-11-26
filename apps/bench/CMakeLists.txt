# The bench: the exe that measures frame times, and its result-file writer.

add_library(bench_output STATIC
        BenchOutput.cpp
        BenchOutput.h)

engine_target_defaults(bench_output)

target_include_directories(bench_output PUBLIC "${PROJECT_SOURCE_DIR}/apps")

target_link_libraries(bench_output PRIVATE
        engine_core)

add_executable(bench
        main.cpp
        BenchOptions.cpp
        BenchOptions.h
        StressGrid.cpp
        StressGrid.h)

# A console exe outside Release, so terminals and IDEs show the log
set_target_properties(bench PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
target_link_options(bench PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(bench)

target_compile_definitions(bench PRIVATE "ENGINE_BUILD_CONFIG=\"$<CONFIG>\"")

target_link_libraries(bench PRIVATE
        app_common
        bench_output
        engine_assets
        engine_core
        engine_rhi
        engine_render
        engine_scene)

add_dependencies(bench compile_shaders)
engine_copy_agility_runtime(bench)
engine_copy_asan_runtime(bench)

add_test(NAME bench_smoke
        COMMAND bench --grid dynamic --count 1000 --frames 5 --warmup 1 --out "${CMAKE_BINARY_DIR}/bench_smoke")
set_tests_properties(bench_smoke PROPERTIES TIMEOUT 30)
