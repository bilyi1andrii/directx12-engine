# engine_ui: the debug UI (Dear ImGui panels; later they write RendererSettings). CPU only: owns the ImGui context and
# its Win32 platform backend; drawing is engine_render's job (UiRenderer). Must never link engine_rhi, engine_render,
# d3d12 or dxgi.

add_library(engine_ui STATIC
        DebugUI.cpp
        DebugUI.h)

engine_target_defaults(engine_ui)

# PUBLIC: users include its headers as "ui/DebugUI.h"
target_include_directories(engine_ui PUBLIC "${PROJECT_SOURCE_DIR}/engine")

# PUBLIC: DebugUI.h uses core::Window. PRIVATE: only DebugUI.cpp includes ImGui
target_link_libraries(engine_ui
        PUBLIC
        engine_core
        PRIVATE
        imgui::imgui)
