# Code shared by the sandbox and the bench: the cube-grid scene, option helpers and the end-of-run checks.
# It sits above the engine, so it may link every engine module.

add_library(app_common STATIC
        AppChecks.cpp
        AppChecks.h
        OptionValues.cpp
        OptionValues.h
        StressScene.cpp
        StressScene.h)

engine_target_defaults(app_common)

# Included as "common/StressScene.h"
target_include_directories(app_common PUBLIC "${PROJECT_SOURCE_DIR}/apps")

# PUBLIC: the headers use render, scene and rhi types
target_link_libraries(app_common
        PUBLIC
        engine_core
        engine_render
        engine_scene
        PRIVATE
        engine_assets)
