# engine_renderdata: the plain CPU data passed between the layers above the renderer and the renderer
# (RenderList, handles, RendererSettings; texture, mesh, material and model data). Scene, assets and UI link
# it instead of engine_render, so they never see the rhi. Namespace engine::render, like the renderer that
# consumes it. CPU only: must never link engine_rhi, d3d12 or dxgi.

add_library(engine_renderdata STATIC
        Handles.h
        MaterialDesc.h
        MeshData.h
        ModelData.h
        RendererSettings.h
        RenderList.cpp
        RenderList.h
        TextureData.h
        Vertex.h)

engine_target_defaults(engine_renderdata)

# PUBLIC: whoever links engine_renderdata can include its headers as "renderdata/RenderList.h"
target_include_directories(engine_renderdata PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_renderdata PUBLIC
        engine_core)
