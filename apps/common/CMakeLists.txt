# app_common: code shared by the apps (sandbox, bench): the stress scene, the frame view from a camera, option
# values and the end-of-run checks. App level, above every engine module, so it may link all of them.

add_library(app_common STATIC
        AppChecks.cpp
        AppChecks.h
        OptionValues.cpp
        OptionValues.h
        StressScene.cpp
        StressScene.h)

engine_target_defaults(app_common)

# PUBLIC: the apps include its headers as "common/StressScene.h"
target_include_directories(app_common PUBLIC "${PROJECT_SOURCE_DIR}/apps")

# PUBLIC: StressScene.h uses render, renderdata and scene types; AppChecks.h uses rhi::ValidationStats
target_link_libraries(app_common
        PUBLIC
        engine_core
        engine_render
        engine_scene
        PRIVATE
        engine_assets)
