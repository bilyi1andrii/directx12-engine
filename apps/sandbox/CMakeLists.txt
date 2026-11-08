add_executable(sandbox
        main.cpp
        SandboxOptions.cpp
        SandboxOptions.h
        SandboxScene.cpp
        SandboxScene.h)

# Equivalent of WIN32
set_target_properties(sandbox PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

# Start console program in wWinMain (needed to show debugger outputs in terminal)
target_link_options(sandbox PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(sandbox)

target_link_libraries(sandbox PRIVATE
        app_common
        # AssetCache, model and texture loading for the models and dropped files
        engine_assets
        engine_core
        engine_rhi
        engine_render
        engine_scene
        engine_ui)

add_dependencies(sandbox compile_shaders copy_data)
engine_copy_agility_runtime(sandbox)
engine_copy_asan_runtime(sandbox)

# Smoke test: renders 120 frames (with two fullscreen toggles) and fails on debug-layer errors, leaks,
# a crash or an early exit. TIMEOUT turns a hang (e.g. a fence that is never signaled) into a failure
# instead of a ctest run that never ends.
add_test(NAME sandbox_smoke COMMAND sandbox --smoke-test 120)
set_tests_properties(sandbox_smoke PROPERTIES TIMEOUT 30)
# The same smoke test with a static grid
add_test(NAME sandbox_smoke_static COMMAND sandbox --smoke-test 120 --grid static)
set_tests_properties(sandbox_smoke_static PROPERTIES TIMEOUT 30)
