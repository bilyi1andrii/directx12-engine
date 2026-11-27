# The sandbox: the interactive app, and its save-file reader and writer, a CPU-only library the unit tests use.

add_library(sandbox_save STATIC
        SceneSave.cpp
        SceneSave.h)

engine_target_defaults(sandbox_save)

target_include_directories(sandbox_save PUBLIC "${PROJECT_SOURCE_DIR}/apps")

target_link_libraries(sandbox_save
        PUBLIC
        engine_core
        PRIVATE
        nlohmann_json::nlohmann_json)

add_executable(sandbox
        main.cpp
        FrameTimeAverages.cpp
        FrameTimeAverages.h
        SandboxLights.cpp
        SandboxLights.h
        SandboxOptions.cpp
        SandboxOptions.h
        SandboxScene.cpp
        SandboxScene.h)

# A console exe outside Release, so terminals and IDEs show the log
set_target_properties(sandbox PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

target_link_options(sandbox PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(sandbox)

target_link_libraries(sandbox PRIVATE
        app_common
        app_shell
        sandbox_save
        engine_assets
        engine_core
        engine_rhi
        engine_render
        engine_scene
        engine_ui)

add_dependencies(sandbox compile_shaders copy_data)
engine_copy_agility_runtime(sandbox)
engine_copy_asan_runtime(sandbox)

add_test(NAME sandbox_smoke COMMAND sandbox --smoke-test 120)
set_tests_properties(sandbox_smoke PROPERTIES TIMEOUT 30)
add_test(NAME sandbox_smoke_static COMMAND sandbox --smoke-test 120 --grid static)
set_tests_properties(sandbox_smoke_static PROPERTIES TIMEOUT 30)
