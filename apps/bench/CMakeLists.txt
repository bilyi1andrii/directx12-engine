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
