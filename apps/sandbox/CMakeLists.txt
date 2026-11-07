add_executable(sandbox
        main.cpp
        SandboxOptions.cpp
        SandboxOptions.h)

# Equivalent of WIN32
set_target_properties(sandbox PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

# Start console program in wWinMain (needed to show debugger outputs in terminal)
target_link_options(sandbox PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(sandbox)

target_link_libraries(sandbox PRIVATE
        engine_assets
        engine_core
        engine_rhi
        engine_render
        engine_scene)

add_dependencies(sandbox compile_shaders)
engine_copy_agility_runtime(sandbox)
engine_copy_asan_runtime(sandbox)

# Smoke test: renders 120 frames (with two fullscreen toggles) and fails on debug-layer errors, leaks,
# a crash or an early exit. TIMEOUT turns a hang (e.g. a fence that is never signaled) into a failure
# instead of a ctest run that never ends.
add_test(NAME sandbox_smoke COMMAND sandbox --smoke-test 120)
set_tests_properties(sandbox_smoke PROPERTIES TIMEOUT 30)

# Stress smoke test: the same checks with 224 x 224 = 50,176 cubes, which take two upload-ring pages (instance chunks),
# so one bucket is split between two chunks. Only 6 frames: GPU-based validation and ASan make every 50k
# frame slow (about 3 s in Debug, 9 s in x64-asan); both fullscreen toggles and both draw paths still run.
add_test(NAME sandbox_smoke_stress COMMAND sandbox --smoke-test 6 --count 50000)
set_tests_properties(sandbox_smoke_stress PROPERTIES TIMEOUT 30)
