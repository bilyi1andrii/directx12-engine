# The renderer library, and the CPU-only part of the frame graph it uses.

add_library(engine_framegraph STATIC
        FrameGraphCompiler.cpp
        FrameGraphCompiler.h)

engine_target_defaults(engine_framegraph)

target_include_directories(engine_framegraph PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_framegraph PUBLIC
        engine_core)

add_library(engine_render STATIC
        DynamicInstances.cpp
        DynamicInstances.h
        FrameGraph.cpp
        FrameGraph.h
        FrameInputs.h
        GpuResources.cpp
        GpuResources.h
        InstanceUpload.h
        Renderer.cpp
        Renderer.h
        TransientTexturePool.cpp
        TransientTexturePool.h
        UiRenderer.cpp
        UiRenderer.h
        passes/DrawConstantPacking.h
        passes/MeshDrawing.h
        passes/OpaquePass.cpp
        passes/OpaquePass.h
        passes/ShadowPass.cpp
        passes/ShadowPass.h
        passes/TransparentPass.cpp
        passes/TransparentPass.h
        passes/UiPass.cpp
        passes/UiPass.h
        passes/VertexLayout.h)

engine_target_defaults(engine_render)

target_include_directories(engine_render PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_include_directories(engine_render PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

target_link_libraries(engine_render PUBLIC
        engine_framegraph
        engine_renderdata
        engine_rhi)
target_link_libraries(engine_render PRIVATE
        imgui::imgui)
