# The debug UI library: ImGui panels that edit plain settings and scene data, and never call the renderer.

add_library(engine_ui STATIC
        DebugUI.cpp
        DebugUI.h
        LightsWindow.cpp
        LightsWindow.h)

engine_target_defaults(engine_ui)

target_include_directories(engine_ui PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_ui
        PUBLIC
        engine_core
        engine_renderdata
        PRIVATE
        imgui::imgui
        imguizmo::imguizmo)
