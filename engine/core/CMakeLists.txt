# engine_core: window, input, timing, logging, error handling

add_library(engine_core STATIC
        Assert.cpp
        Assert.h
        CommandLine.cpp
        CommandLine.h
        ComScope.cpp
        ComScope.h
        Error.h
        File.cpp
        File.h
        Input.cpp
        Input.h
        IntervalAverage.cpp
        IntervalAverage.h
        Log.cpp
        Log.h
        ThrowIfFailed.cpp
        ThrowIfFailed.h
        Timer.cpp
        Timer.h
        UniqueHandle.h
        Utf8.cpp
        Utf8.h
        Window.cpp
        Window.h)

engine_target_defaults(engine_core)

# PUBLIC: whoever links engine_core can include its headers as "core/Window.h, ..."
target_include_directories(engine_core PUBLIC "${PROJECT_SOURCE_DIR}/engine")

# Windows SDK libraries: CommandLineToArgvW and CoInitializeEx. CMake links them by default with MSVC; listed so
# the dependency is visible. PRIVATE still reaches the final link, because engine_core is a static library.
target_link_libraries(engine_core PRIVATE shell32 ole32)