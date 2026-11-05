# engine_tests: unit tests of the CPU-only modules. Must stay free of D3D12 and need no GPU.

add_executable(engine_tests
        main.cpp
        FileTests.cpp
        TestFiles.h
        TextureLoaderTests.cpp)

engine_target_defaults(engine_tests)

target_link_libraries(engine_tests PRIVATE
        engine_core
        engine_assets
        GTest::gtest) # not gtest_main: main.cpp holds the process-wide core::ComScope

# Before gtest_discover_tests: discovery runs the exe at build time, so the runtime must already be next to it
engine_copy_asan_runtime(engine_tests)
gtest_discover_tests(engine_tests)
