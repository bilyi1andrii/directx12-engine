# The core library: the window, input, timing, logging and error handling the rest of the engine uses.

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

target_include_directories(engine_core PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_core PRIVATE shell32 ole32)
