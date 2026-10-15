# engine_core: window, input, timing, logging, error handling

add_library(engine_core STATIC
        Assert.cpp
        Assert.h
        Input.cpp
        Input.h
        Log.cpp
        Log.h
        ThrowIfFailed.cpp
        ThrowIfFailed.h
        Timer.cpp
        Timer.h
        UniqueHandle.h
        Window.cpp
        Window.h)

engine_target_defaults(engine_core)

# PUBLIC: whoever links engine_core can include its headers as "core/Window.h, ..."
target_include_directories(engine_core PUBLIC "${PROJECT_SOURCE_DIR}/engine")