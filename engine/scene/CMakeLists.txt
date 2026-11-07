# engine_scene: what is drawn and from where (Camera, CameraPath, Picking; later Scene, Transform, MeshRenderer,
# Light). CPU only: must never link engine_rhi, d3d12 or dxgi.

add_library(engine_scene STATIC
        Camera.cpp
        Camera.h
        CameraController.cpp
        CameraController.h
        CameraPath.cpp
        CameraPath.h
        Picking.cpp
        Picking.h)

engine_target_defaults(engine_scene)

target_include_directories(engine_scene PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_scene PUBLIC
        engine_core)
