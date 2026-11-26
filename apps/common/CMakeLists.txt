# Code the sandbox and the bench share: the grid settings, option helpers and the end-of-run checks.

add_library(app_common STATIC
        AppChecks.cpp
        AppChecks.h
        GridSetup.cpp
        GridSetup.h
        OptionValues.cpp
        OptionValues.h)

engine_target_defaults(app_common)

target_include_directories(app_common PUBLIC "${PROJECT_SOURCE_DIR}/apps")

target_link_libraries(app_common
        PUBLIC
        engine_core
        engine_rhi)
