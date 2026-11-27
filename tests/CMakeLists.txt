# engine_tests: unit tests of the CPU-only code (engine modules and the bench's result files). Must stay free of
# D3D12 and need no GPU.

add_executable(engine_tests
        main.cpp
        AssetCacheTests.cpp
        BenchOutputTests.cpp
        CameraControllerTests.cpp
        CameraPathTests.cpp
        CameraTests.cpp
        FileTests.cpp
        FrameGraphCompilerTests.cpp
        GpuTimeCollectorTests.cpp
        GpuTimesTests.cpp
        GridLayoutTests.cpp
        InstanceChunksTests.cpp
        LightPackingTests.cpp
        ModelImporterTests.cpp
        PickingTests.cpp
        PrimitivesTests.cpp
        RenderListTests.cpp
        SceneSaveTests.cpp
        SceneTests.cpp
        ShadowViewTests.cpp
        TestFiles.h
        TextureLoaderTests.cpp
        TransparentSortTests.cpp
        Utf8Tests.cpp)

engine_target_defaults(engine_tests)

target_include_directories(engine_tests PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

target_link_libraries(engine_tests PRIVATE
        bench_output
        sandbox_save
        engine_core
        engine_assets
        engine_framegraph
        engine_gputimes
        engine_renderdata
        engine_scene
        GTest::gtest) # not gtest_main: main.cpp holds the process-wide core::ComScope

# Before gtest_discover_tests: discovery runs the exe at build time
engine_copy_asan_runtime(engine_tests)
gtest_discover_tests(engine_tests)
