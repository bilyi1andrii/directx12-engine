# Code the sandbox and the bench share: the grid settings, option helpers, the start and end of every app and the
# end-of-run checks (app_common), and the window, input, debug UI and renderer every app runs in (app_shell).

add_library(app_common STATIC
        AppChecks.cpp
        AppChecks.h
        AppMain.cpp
        AppMain.h
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

add_library(app_shell STATIC
        AppShell.cpp
        AppShell.h)

engine_target_defaults(app_shell)

target_include_directories(app_shell PUBLIC "${PROJECT_SOURCE_DIR}/apps")

target_link_libraries(app_shell
        PUBLIC
        engine_core
        engine_render
        engine_rhi
        engine_ui)
