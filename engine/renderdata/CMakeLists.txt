# engine_renderdata: plain CPU data the renderer draws from (RenderList, handles, settings, texture, mesh and
# material data). Scene, assets and UI link this instead of engine_render. Namespace engine::render.
# Must never link engine_rhi, d3d12 or dxgi.

add_library(engine_renderdata STATIC
        FrameView.h
        GpuUploader.h
        GridLayout.cpp
        GridLayout.h
        Handles.h
        InstanceChunks.cpp
        InstanceChunks.h
        MaterialDesc.h
        MeshData.h
        Model.h
        ModelData.h
        RendererSettings.h
        RenderList.cpp
        RenderList.h
        TextureData.h
        Vertex.h)

engine_target_defaults(engine_renderdata)

# PUBLIC: users include its headers as "renderdata/RenderList.h"
target_include_directories(engine_renderdata PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_renderdata PUBLIC
        engine_core)
