# The scene library: objects, cameras and picking, on the CPU only.

add_library(engine_scene STATIC
        Camera.cpp
        Camera.h
        CameraController.cpp
        CameraController.h
        CameraPath.cpp
        CameraPath.h
        Picking.cpp
        Picking.h
        Scene.cpp
        Scene.h)

engine_target_defaults(engine_scene)

target_include_directories(engine_scene PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_scene PUBLIC
        engine_core
        engine_renderdata)
